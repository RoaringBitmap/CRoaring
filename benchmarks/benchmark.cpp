// Unified CRoaring benchmark. One binary, many named benchmarks, selectable
// with --filter. Measurements use lemire/counters
// (https://github.com/lemire/counters) for hardware performance counters
// when available (Linux perf_event; Apple Silicon with kpc entitlement),
// falling back to wall-clock timing otherwise.
//
// Each registered benchmark provides:
//   - setup():    builds fresh state (called once per measured iteration)
//   - run():      the workload; returns an answer for optional correctness
//   check
//   - teardown(): frees state
//   - ops_per_run: operations performed in a single run() call
//   - inner_reps:  how many back-to-back calls to run() fit in one timed
//                  window, used to push very short kernels above clock
//                  resolution. Stateful benchmarks leave this at 1.
//
// Replaces the per-file benchmarks that used BEST_TIME / RDTSC_* macros.
// Notable bug fixes vs. the old benchmarks:
//   * add/set and remove/unset no longer collapse to no-ops after the first
//     iteration (the old harness rebuilt state only once).
//   * prefetch/flush contains benchmarks now use actually-populated queries.
//   * real_bitmaps contains queries land in each bitmap's own range (not the
//     global max, which previously often fell outside).
//   * reported op counts match the actual workload rather than TESTSIZE.

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <roaring/containers/array.h>
#include <roaring/containers/bitset.h>
#include <roaring/containers/convert.h>
#include <roaring/containers/mixed_equal.h>
#include <roaring/containers/run.h>
#include <roaring/misc/configreport.h>
#include <roaring/portability.h>
#include <roaring/roaring.h>
#include <roaring/roaring64.h>
#include <roaring/roaring64map.hh>

#include "counters/event_counter.h"
#include "numbersfromtextfiles.h"
#include "random.h"

// When the roaring headers are compiled as C++, the internal container
// symbols live in roaring::internal.
using namespace roaring::internal;
using roaring::Roaring64Map;
using roaring::misc::tellmeall;

namespace {

// ---------------------------------------------------------------- registry

struct Entry {
    std::string name;
    std::string description;
    std::function<void *()> setup;
    std::function<int64_t(void *)> run;
    std::function<void(void *)> teardown;
    int64_t ops_per_run = 1;
    int64_t inner_reps = 1;
    int64_t expected = 0;
    bool check_expected = false;
    // When true, setup runs once and state is shared across all iterations
    // (including inner_reps and warmup). Use for benchmarks where setup is
    // expensive and the workload does not mutate shared state.
    bool reusable_state = false;
};

// Print text wrapped at `width` columns with a 4-space indent — used by
// --list-description so the prose stays readable on a standard terminal.
void print_wrapped(const std::string &text, size_t width = 72,
                   const char *indent = "    ") {
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ') ++i;
        if (i >= text.size()) break;
        size_t start = i;
        size_t last_space = std::string::npos;
        while (i < text.size() && (i - start) < width) {
            if (text[i] == ' ') last_space = i;
            ++i;
        }
        size_t end;
        if (i >= text.size()) {
            end = text.size();
        } else if (last_space != std::string::npos && last_space > start) {
            end = last_space;
            i = last_space + 1;
        } else {
            end = i;  // very long word — break on the boundary
        }
        printf("%s%s\n", indent, text.substr(start, end - start).c_str());
    }
}

struct RunResult {
    counters::event_aggregate agg;
    int64_t last_answer;
    bool answer_ok;
};

RunResult run_entry(const Entry &e, int min_iter, double min_ns, int max_iter,
                    bool use_counters) {
    // On Apple Silicon, the counters library's kpc setup grabs a
    // process-wide counter lock via kpc_force_all_ctrs_set(1); the
    // AppleEvents setup_performance_counters() is idempotent per
    // instance (an `init` flag short-circuits the second call), so we
    // reuse one thread-local collector per thread. That way the kpc
    // grab happens exactly once per thread and every run_entry call
    // skips the expensive kpep setup. On Linux the collector holds
    // five perf_event_open fds for the thread's lifetime. The collector
    // is intentionally leaked on thread exit — the OS reclaims the fds.
    static thread_local counters::event_collector coll;
    counters::event_aggregate agg;

    // For reusable_state benchmarks, build state once and share it.
    void *shared_state = nullptr;
    if (e.reusable_state && e.setup) {
        shared_state = e.setup();
        // Untimed warmup on the shared state.
        for (int64_t j = 0; j < e.inner_reps; ++j) (void)e.run(shared_state);
    } else if (e.setup) {
        // Untimed warmup — populate caches and let the allocator warm up.
        void *state = e.setup();
        for (int64_t j = 0; j < e.inner_reps; ++j) (void)e.run(state);
        if (e.teardown) e.teardown(state);
    }

    int iter = 0;
    int64_t last_answer = 0;
    bool answer_ok = true;
    double total_ns = 0.0;

    while (iter < min_iter || (total_ns < min_ns && iter < max_iter)) {
        void *state = nullptr;
        if (e.reusable_state) {
            state = shared_state;
        } else if (e.setup) {
            state = e.setup();
        }
        counters::event_count c;
        int64_t r = 0;
        if (use_counters) {
            coll.start();
            for (int64_t j = 0; j < e.inner_reps; ++j) r = e.run(state);
            c = coll.end();
        } else {
            // Parallel mode: counters are meaningless so we just time with
            // CLOCK_MONOTONIC directly and zero the rest of the aggregate.
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (int64_t j = 0; j < e.inner_reps; ++j) r = e.run(state);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double ns = static_cast<double>(t1.tv_sec - t0.tv_sec) * 1e9 +
                        static_cast<double>(t1.tv_nsec - t0.tv_nsec);
            c.elapsed = std::chrono::duration<double>(ns / 1e9);
        }
        if (!e.reusable_state && e.teardown) e.teardown(state);

        agg << c;
        last_answer = r;
        if (e.check_expected && r != e.expected) answer_ok = false;
        total_ns += c.elapsed_ns();
        ++iter;
    }
    if (e.reusable_state && e.teardown) e.teardown(shared_state);

    // We intentionally do NOT destroy the thread-local collector here —
    // on Apple Silicon that would release the kpc lock and force the
    // next benchmark to pay the full kpep setup (which then fails).
    agg.has_events = use_counters ? coll.has_events() : false;
    RunResult rr;
    rr.agg = agg;
    rr.last_answer = last_answer;
    rr.answer_ok = answer_ok;
    return rr;
}

enum class OutputFormat { Markdown, Csv };

// Fixed column widths for Markdown output. The benchmark-name column is
// sized dynamically from the longest selected name so each row is visually
// aligned in raw text (Markdown renderers tolerate any amount of padding).
struct MdLayout {
    int name;
    int ns = 10;
    int cyc = 10;
    int ghz = 6;
    int ins = 10;
    int ipc = 8;
    int brm = 10;
    int miss = 10;
};

MdLayout compute_md_layout(const std::vector<const Entry *> &selected) {
    MdLayout L;
    size_t w = std::strlen("benchmark");
    for (const Entry *ep : selected) {
        w = std::max(w, ep->name.size());
    }
    L.name = static_cast<int>(w);
    return L;
}

// Emit `n` copies of character `c`.
std::string repeat(char c, int n) {
    return std::string(n > 0 ? static_cast<size_t>(n) : 0, c);
}

void print_header(bool has_counters, OutputFormat fmt, const MdLayout &L) {
    if (fmt == OutputFormat::Csv) {
        if (has_counters) {
            printf(
                "benchmark,ns_per_op,cyc_per_op,ghz,ins_per_op,ins_per_cyc,"
                "brm_per_op,miss_per_op,answer_ok\n");
        } else {
            printf("benchmark,ns_per_op,answer_ok\n");
        }
        return;
    }
    // Markdown table — fixed widths, left-aligned name, right-aligned
    // numerics. The separator row uses a trailing colon to right-align the
    // numeric columns in renderers; raw-text readers see aligned columns.
    if (has_counters) {
        printf("| %-*s | %*s | %*s | %*s | %*s | %*s | %*s | %*s |\n", L.name,
               "benchmark", L.ns, "ns/op", L.cyc, "cyc/op", L.ghz, "GHz", L.ins,
               "ins/op", L.ipc, "ins/cyc", L.brm, "brm/op", L.miss, "miss/op");
        printf("|%s|%s:|%s:|%s:|%s:|%s:|%s:|%s:|\n",
               repeat('-', L.name + 2).c_str(), repeat('-', L.ns + 1).c_str(),
               repeat('-', L.cyc + 1).c_str(), repeat('-', L.ghz + 1).c_str(),
               repeat('-', L.ins + 1).c_str(), repeat('-', L.ipc + 1).c_str(),
               repeat('-', L.brm + 1).c_str(), repeat('-', L.miss + 1).c_str());
    } else {
        printf("| %-*s | %*s |\n", L.name, "benchmark", L.ns, "ns/op");
        printf("|%s|%s:|\n", repeat('-', L.name + 2).c_str(),
               repeat('-', L.ns + 1).c_str());
    }
}

// Escape a benchmark name for embedding in a Markdown table cell. The only
// benchmark-name characters that matter inside a pipe-delimited row are `|`
// and `\` (plus underscores, which Markdown renderers sometimes italicise).
// We escape `|` and `\` as \| and \\ respectively and leave the rest alone.
std::string md_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '|' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

// Escape a benchmark name for a CSV cell. Names in this benchmark do not
// contain commas, newlines, or quotes today, but be defensive: if any of
// those appear, wrap the cell in quotes and double embedded quotes.
std::string csv_escape(const std::string &s) {
    bool needs_quote = s.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs_quote) return s;
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

void print_row(const Entry &e, const RunResult &r, bool has_counters,
               OutputFormat fmt, const MdLayout &L) {
    double ops =
        static_cast<double>(e.ops_per_run) * static_cast<double>(e.inner_reps);
    if (ops <= 0) ops = 1;
    const auto &a = r.agg;
    double best_ns_total = a.fastest_elapsed_ns();
    double best_cyc_total = a.fastest_cycles();
    double best_ins_total = a.fastest_instructions();
    double best_brm_total = a.fastest_branch_misses();
    double ns = best_ns_total / ops;
    double cyc = best_cyc_total / ops;
    double ins = best_ins_total / ops;
    double brm = best_brm_total / ops;
    double miss = a.fastest_cache_misses() / ops;
    // GHz and IPC are derived from the best-iteration totals.
    double ghz = (best_ns_total > 0.0) ? (best_cyc_total / best_ns_total) : 0.0;
    double ipc =
        (best_cyc_total > 0.0) ? (best_ins_total / best_cyc_total) : 0.0;

    if (fmt == OutputFormat::Csv) {
        const char *ok = r.answer_ok ? "1" : "0";
        if (has_counters) {
            printf("%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.6f,%.6f,%s\n",
                   csv_escape(e.name).c_str(), ns, cyc, ghz, ins, ipc, brm,
                   miss, ok);
        } else {
            printf("%s,%.4f,%s\n", csv_escape(e.name).c_str(), ns, ok);
        }
        return;
    }

    // Markdown — fixed-width columns. The bad-answer tag is appended to the
    // name cell; it may push a single row past the computed column width,
    // which the renderer still accepts (the tag is rare and loud by design).
    std::string name = md_escape(e.name);
    if (!r.answer_ok) name += " **[BAD ANSWER]**";
    if (has_counters) {
        printf(
            "| %-*s | %*.2f | %*.2f | %*.2f | %*.2f | %*.2f | %*.4f | %*.4f "
            "|\n",
            L.name, name.c_str(), L.ns, ns, L.cyc, cyc, L.ghz, ghz, L.ins, ins,
            L.ipc, ipc, L.brm, brm, L.miss, miss);
    } else {
        printf("| %-*s | %*.2f |\n", L.name, name.c_str(), L.ns, ns);
    }
}

// ----------------------------------------------------------------- helpers

// 1/3 density: every third value in [0, 2^16).
constexpr int kArrayStride = 3;
constexpr int kArrayCount = (1 << 16) / kArrayStride + 1;

void populate_array_stride(array_container_t *B, int stride) {
    for (int x = 0; x < (1 << 16); x += stride) {
        array_container_add(B, static_cast<uint16_t>(x));
    }
}
void populate_bitset_stride(bitset_container_t *B, int stride) {
    for (int x = 0; x < (1 << 16); x += stride) {
        bitset_container_set(B, static_cast<uint16_t>(x));
    }
}
void populate_run_stride(run_container_t *B, int stride) {
    for (int x = 0; x < (1 << 16); x += stride) {
        run_container_add(B, static_cast<uint16_t>(x));
    }
}

// --------------------------------------------- array_container benches

struct ArrayState {
    array_container_t *B;
};
struct ArrayPairState {
    array_container_t *B1;
    array_container_t *B2;
    array_container_t *BO;
    int32_t input_total;
};

void register_array_container(std::vector<Entry> &out) {
    {
        Entry e;
        e.name = "array_container/add";
        e.description =
            "Inserts every third 16-bit value in [0, 2^16) into an "
            "array_container_t, measuring the amortised cost of "
            "array_container_add() when the input is already in ascending "
            "order (so each insertion appends to the tail of the sorted "
            "array, avoiding any shifting). The container is reset to "
            "cardinality 0 at the start of every run() call so the inner-"
            "repeat loop genuinely re-populates each time. The old "
            "BEST_TIME benchmark only ran the populate once and then "
            "re-checked existing membership for the remaining 499 "
            "iterations.";
        e.setup = []() -> void * {
            auto *s = new ArrayState;
            s->B = array_container_create();
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<ArrayState *>(sv);
            // Reset to empty (the storage stays allocated; cardinality
            // becomes 0). This makes the inner-repeat loop measure the
            // populate work each time rather than degenerating into
            // no-op contains() probes after the first call.
            s->B->cardinality = 0;
            populate_array_stride(s->B, kArrayStride);
            return array_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<ArrayState *>(sv);
            array_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        e.inner_reps = 50;
        e.expected = kArrayCount;
        e.check_expected = true;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "array_container/contains_all_u16";
        e.description =
            "Issues 65536 membership queries (one for every 16-bit value) "
            "against an array_container_t holding every third value in "
            "[0, 2^16). Each query is a branchless binary search in the "
            "sorted value array. The container stays hot in L1 cache "
            "across queries, so the result reports best-case "
            "array_container_contains() throughput.";
        e.setup = []() -> void * {
            auto *s = new ArrayState;
            s->B = array_container_create();
            populate_array_stride(s->B, kArrayStride);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<ArrayState *>(sv);
            int64_t card = 0;
            for (int x = 0; x < (1 << 16); ++x) {
                card +=
                    array_container_contains(s->B, static_cast<uint16_t>(x));
            }
            return card;
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<ArrayState *>(sv);
            array_container_free(s->B);
            delete s;
        };
        e.ops_per_run = 1 << 16;
        e.inner_reps = 20;
        e.expected = kArrayCount;
        e.check_expected = true;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "array_container/remove";
        e.description =
            "Removes every stored value from an array_container_t holding "
            "every third 16-bit value. Values are removed in ascending "
            "order, so each call to array_container_remove() deletes the "
            "first element and shifts every remaining element down by one "
            "— the total cost is therefore quadratic in the cardinality. "
            "The container is repopulated before every timed iteration, "
            "which fixes the old BEST_TIME benchmark where iterations two "
            "through five hundred operated on an already-empty container "
            "and produced the infamous 0.01 cycles-per-op figure.";
        e.setup = []() -> void * {
            auto *s = new ArrayState;
            s->B = array_container_create();
            populate_array_stride(s->B, kArrayStride);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<ArrayState *>(sv);
            for (int x = 0; x < (1 << 16); x += kArrayStride) {
                array_container_remove(s->B, static_cast<uint16_t>(x));
            }
            return array_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<ArrayState *>(sv);
            array_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        e.expected = 0;
        e.check_expected = true;
        out.push_back(std::move(e));
    }
    {
        struct S {
            array_container_t *B;
            uint32_t *out;
        };
        Entry e;
        e.name = "array_container/to_uint32_array";
        e.description =
            "Converts an array_container_t holding every third 16-bit "
            "value into a flat uint32_t array, prefixing each stored 16-bit "
            "key with a constant 32-bit high word (1234 << 16). This "
            "mirrors the hot path for roaring_bitmap_to_uint32_array when "
            "emitting a container's values into the user-facing buffer.";
        e.setup = []() -> void * {
            auto *s = new S;
            s->B = array_container_create();
            populate_array_stride(s->B, kArrayStride);
            s->out = static_cast<uint32_t *>(
                malloc(sizeof(uint32_t) * array_container_cardinality(s->B)));
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            return array_container_to_uint32_array(s->out, s->B, 1234);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<S *>(sv);
            free(s->out);
            array_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        e.inner_reps = 50;
        e.expected = kArrayCount;
        e.check_expected = true;
        out.push_back(std::move(e));
    }

    // Fix: the old benchmark never populated its query array, so contains was
    // effectively called with zero for every query (likely constant-folded).
    {
        struct S {
            array_container_t *B;
            std::vector<uint16_t> queries;
        };
        constexpr size_t kQueries = 1024;
        auto setup = []() -> void * {
            auto *s = new S;
            s->B = array_container_create();
            populate_array_stride(s->B, kArrayStride);
            s->queries.resize(kQueries);
            for (size_t i = 0; i < kQueries; ++i) {
                s->queries[i] = static_cast<uint16_t>(pcg32_random());
            }
            return s;
        };
        auto teardown = [](void *sv) {
            auto *s = static_cast<S *>(sv);
            array_container_free(s->B);
            delete s;
        };

        {
            Entry e;
            e.name = "array_container/contains_random_prefetch";
            e.description =
                "Runs 1024 membership queries at uniformly random 16-bit "
                "values against a populated array_container_t. Before "
                "every probe, the container's value array is fully "
                "prefetched (via __builtin_prefetch on every cache line), "
                "modelling the hot-cache regime where the container has "
                "just been touched and every lookup hits L1. This fixes "
                "the old benchmark, whose query array was never populated, "
                "so every probe really asked contains(0).";
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                for (uint16_t q : s->queries) {
#if !CROARING_REGULAR_VISUAL_STUDIO
                    for (int32_t k = 0; k < s->B->cardinality;
                         k += 64 / (int32_t)sizeof(uint16_t)) {
                        __builtin_prefetch(s->B->array + k);
                    }
#endif
                    hits += array_container_contains(s->B, q);
                }
                return hits;
            };
            e.teardown = teardown;
            e.ops_per_run = static_cast<int64_t>(kQueries);
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "array_container/contains_random_flush";
            e.description =
                "Runs 1024 membership queries at uniformly random 16-bit "
                "values against a populated array_container_t. Before "
                "every probe, the container's value array is evicted from "
                "cache by issuing clflush on every cache line (x86 only; "
                "a no-op on other architectures), modelling the cold-cache "
                "regime where each lookup pays the full memory latency. "
                "Like the prefetch variant, this fixes the old benchmark "
                "whose query array was never populated.";
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                for (uint16_t q : s->queries) {
#if defined(CROARING_IS_X64) && !(defined(_MSC_VER) && !defined(__clang__))
                    for (int32_t k = 0; k < s->B->cardinality;
                         k += 64 / (int32_t)sizeof(uint16_t)) {
                        __builtin_ia32_clflush(s->B->array + k);
                    }
#endif
                    hits += array_container_contains(s->B, q);
                }
                return hits;
            };
            e.teardown = teardown;
            e.ops_per_run = static_cast<int64_t>(kQueries);
            e.inner_reps = 20;
            out.push_back(std::move(e));
        }
    }

    // Union / intersection across two densities. Two pair flavors:
    //   stride3_stride5 — dense containers, large output.
    //   stride16_pow2   — sparse containers, tiny output (bounds-stress).
    auto register_pair = [&](const char *tag, const char *pair_descr,
                             int stride1, int stride2, bool pow2_b2) {
        auto build_pair = [stride1, stride2, pow2_b2]() {
            auto *s = new ArrayPairState;
            s->B1 = array_container_create();
            s->B2 = array_container_create();
            s->BO = array_container_create();
            for (int x = 0; x < (1 << 16); x += stride1) {
                array_container_add(s->B1, static_cast<uint16_t>(x));
            }
            if (pow2_b2) {
                for (int x = 1; x < (1 << 16); x += x) {
                    array_container_add(s->B2, static_cast<uint16_t>(x));
                }
            } else {
                for (int x = 0; x < (1 << 16); x += stride2) {
                    array_container_add(s->B2, static_cast<uint16_t>(x));
                }
            }
            s->input_total = s->B1->cardinality + s->B2->cardinality;
            return s;
        };
        auto pair_teardown = [](void *sv) {
            auto *s = static_cast<ArrayPairState *>(sv);
            array_container_free(s->B1);
            array_container_free(s->B2);
            array_container_free(s->BO);
            delete s;
        };

        int32_t input_total = 0;
        {
            auto *probe = build_pair();
            input_total = probe->input_total;
            pair_teardown(probe);
        }

        {
            Entry e;
            e.name = std::string("array_container/union_") + tag;
            e.description =
                std::string(
                    "Computes the set union of two array_container_t "
                    "objects via array_container_union(), writing the "
                    "result into a third output container. ") +
                pair_descr +
                " Reported cost is per input element (|B1| + |B2|), and "
                "the run() kernel is repeated 200 times inside one timing "
                "window to stay above clock resolution for the dense "
                "variants.";
            e.setup = [build_pair]() -> void * { return build_pair(); };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<ArrayPairState *>(sv);
                array_container_union(s->B1, s->B2, s->BO);
                return s->BO->cardinality;
            };
            e.teardown = pair_teardown;
            e.ops_per_run = input_total;
            e.inner_reps = 2000;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = std::string("array_container/intersection_") + tag;
            e.description =
                std::string(
                    "Computes the set intersection of two array_container_t "
                    "objects via array_container_intersection(), writing "
                    "the result into a third output container. ") +
                pair_descr +
                " Reported cost is per input element (|B1| + |B2|), and "
                "the run() kernel is repeated 200 times inside one timing "
                "window to stay above clock resolution for the sparse "
                "variants.";
            e.setup = [build_pair]() -> void * { return build_pair(); };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<ArrayPairState *>(sv);
                array_container_intersection(s->B1, s->B2, s->BO);
                return s->BO->cardinality;
            };
            e.teardown = pair_teardown;
            e.ops_per_run = input_total;
            e.inner_reps = 2000;
            out.push_back(std::move(e));
        }
    };

    register_pair(
        "stride3_stride5",
        "B1 holds every third 16-bit value (cardinality ~21846) and B2 every "
        "fifth (cardinality ~13108); both are dense array containers above "
        "the typical array->bitset switchover, stressing the merge code.",
        3, 5, false);
    register_pair(
        "stride16_pow2",
        "B1 holds every sixteenth 16-bit value (cardinality 4096) and B2 "
        "holds only the 16 powers of two below 2^16. The pair is very "
        "sparse with a tiny output, exercising the short-input code paths "
        "and boundary handling.",
        16, 0, true);
}

// --------------------------------------------- bitset_container benches

struct BitsetState {
    bitset_container_t *B;
};

void register_bitset_container(std::vector<Entry> &out) {
    {
        Entry e;
        e.name = "bitset_container/set";
        e.description =
            "Sets every third bit in a freshly allocated bitset_container_t "
            "(a 65536-bit fixed-size bitmap), measuring the per-bit cost "
            "of bitset_container_set(): compute the word index, OR the "
            "appropriate mask, and update the cached cardinality. A fresh "
            "container is created per iteration so every bit is actually "
            "flipped from 0 to 1.";
        e.setup = []() -> void * {
            auto *s = new BitsetState;
            s->B = bitset_container_create();
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<BitsetState *>(sv);
            // Reset the bitmap to all-zeros so the inner-repeat loop
            // genuinely measures bit-setting work each time (not just
            // no-op redundant sets of bits already at 1).
            std::memset(s->B->words, 0,
                        sizeof(uint64_t) * BITSET_CONTAINER_SIZE_IN_WORDS);
            s->B->cardinality = 0;
            populate_bitset_stride(s->B, 3);
            return bitset_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<BitsetState *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        e.inner_reps = 20;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "bitset_container/get_all_u16";
        e.description =
            "Issues 65536 membership queries (one per 16-bit value) "
            "against a bitset_container_t holding every third bit. Each "
            "query is a word-load and mask-and-test — the whole workload "
            "streams through all 1024 words of the bitmap once, so this "
            "primarily measures L1 throughput on the word array.";
        e.setup = []() -> void * {
            auto *s = new BitsetState;
            s->B = bitset_container_create();
            populate_bitset_stride(s->B, 3);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<BitsetState *>(sv);
            int64_t card = 0;
            for (int x = 0; x < (1 << 16); ++x) {
                card += bitset_container_get(s->B, static_cast<uint16_t>(x));
            }
            return card;
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<BitsetState *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        e.ops_per_run = 1 << 16;
        e.inner_reps = 20;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "bitset_container/cardinality";
        e.description =
            "Reads the cached cardinality field of a bitset_container_t. "
            "This is a trivial load and should be essentially free — the "
            "inner-repeat loop runs the accessor 100,000 times per timed "
            "window so the result stays above the clock's resolution and "
            "the kpc-read overhead is amortised.";
        e.setup = []() -> void * {
            auto *s = new BitsetState;
            s->B = bitset_container_create();
            populate_bitset_stride(s->B, 3);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<BitsetState *>(sv);
            return bitset_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<BitsetState *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        e.ops_per_run = 1;
        e.inner_reps = 100000;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "bitset_container/compute_cardinality";
        e.description =
            "Recomputes the cardinality of a 65536-bit bitset from "
            "scratch by running popcount over all 1024 u64 words. Reported "
            "cost is per word, so you can compare directly against the "
            "hardware popcount throughput of the target machine.";
        e.setup = []() -> void * {
            auto *s = new BitsetState;
            s->B = bitset_container_create();
            populate_bitset_stride(s->B, 3);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<BitsetState *>(sv);
            return bitset_container_compute_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<BitsetState *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        e.ops_per_run = BITSET_CONTAINER_SIZE_IN_WORDS;
        e.inner_reps = 500;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "bitset_container/unset";
        e.description =
            "Clears every third bit in a pre-populated bitset_container_t "
            "via bitset_container_remove(). The container is repopulated "
            "before every timed iteration, so each clear operation is "
            "actually flipping a 1 bit to 0 and decrementing the cached "
            "cardinality — the old BEST_TIME benchmark ran this exactly "
            "once and reported noise for the other 499 iterations.";
        e.setup = []() -> void * {
            auto *s = new BitsetState;
            s->B = bitset_container_create();
            populate_bitset_stride(s->B, 3);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<BitsetState *>(sv);
            for (int x = 0; x < (1 << 16); x += 3) {
                bitset_container_remove(s->B, static_cast<uint16_t>(x));
            }
            return bitset_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<BitsetState *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        out.push_back(std::move(e));
    }

    // Random-probe contains variants (queries are actually populated).
    {
        struct S {
            bitset_container_t *B;
            std::vector<uint16_t> queries;
        };
        constexpr size_t kQueries = 1024;
        auto setup = []() -> void * {
            auto *s = new S;
            s->B = bitset_container_create();
            while (bitset_container_cardinality(s->B) < 16384) {
                bitset_container_set(s->B,
                                     static_cast<uint16_t>(pcg32_random()));
            }
            s->queries.resize(kQueries);
            for (size_t i = 0; i < kQueries; ++i) {
                s->queries[i] = static_cast<uint16_t>(pcg32_random());
            }
            return s;
        };
        auto teardown = [](void *sv) {
            auto *s = static_cast<S *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        {
            Entry e;
            e.name = "bitset_container/get_random_prefetch";
            e.description =
                "Runs 1024 random-bit membership queries against a "
                "bitset_container_t holding ~16384 randomly-set bits. "
                "Before every probe, the full 8 KiB word array is "
                "prefetched (__builtin_prefetch on every cache line), "
                "modelling the hot-cache regime. The old benchmark "
                "silently queried an uninitialised array; this version "
                "uses pcg32 to pick real random 16-bit positions.";
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                for (uint16_t q : s->queries) {
#if !CROARING_REGULAR_VISUAL_STUDIO
                    for (int32_t k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS;
                         k += 64 / (int32_t)sizeof(uint64_t)) {
                        __builtin_prefetch(s->B->words + k);
                    }
#endif
                    hits += bitset_container_get(s->B, q);
                }
                return hits;
            };
            e.teardown = teardown;
            e.ops_per_run = static_cast<int64_t>(kQueries);
            e.inner_reps = 20;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "bitset_container/get_random_flush";
            e.description =
                "Runs 1024 random-bit membership queries against a "
                "bitset_container_t holding ~16384 randomly-set bits. "
                "Before every probe, the full 8 KiB word array is "
                "evicted from cache by issuing clflush on every cache "
                "line (x86 only; no-op elsewhere), modelling the "
                "cold-cache regime where every lookup pays full DRAM "
                "latency. Queries use pcg32 random 16-bit positions.";
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                for (uint16_t q : s->queries) {
#if defined(CROARING_IS_X64) && !(defined(_MSC_VER) && !defined(__clang__))
                    for (int32_t k = 0; k < BITSET_CONTAINER_SIZE_IN_WORDS;
                         k += 64 / (int32_t)sizeof(uint64_t)) {
                        __builtin_ia32_clflush(s->B->words + k);
                    }
#endif
                    hits += bitset_container_get(s->B, q);
                }
                return hits;
            };
            e.teardown = teardown;
            e.ops_per_run = static_cast<int64_t>(kQueries);
            e.inner_reps = 100;
            out.push_back(std::move(e));
        }
    }

    // Logical ops: AND / OR / justcard / nocard / compute_cardinality.
    {
        struct S {
            bitset_container_t *B1;
            bitset_container_t *B2;
            bitset_container_t *BO;
        };
        auto build = []() -> void * {
            auto *s = new S;
            s->B1 = bitset_container_create();
            s->B2 = bitset_container_create();
            s->BO = bitset_container_create();
            populate_bitset_stride(s->B1, 3);
            populate_bitset_stride(s->B2, 5);
            return s;
        };
        auto td = [](void *sv) {
            auto *s = static_cast<S *>(sv);
            bitset_container_free(s->B1);
            bitset_container_free(s->B2);
            bitset_container_free(s->BO);
            delete s;
        };
        using Fn = std::function<int64_t(S *)>;
        auto add_bench = [&](const char *name, const char *descr, Fn fn,
                             int64_t ops_per_run) {
            Entry e;
            e.name = name;
            e.description = descr;
            e.setup = build;
            e.run = [fn](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                return fn(s);
            };
            e.teardown = td;
            e.ops_per_run = ops_per_run;
            e.inner_reps = 500;
            out.push_back(std::move(e));
        };
        const int64_t N = BITSET_CONTAINER_SIZE_IN_WORDS;
        add_bench(
            "bitset_container/and",
            "Pointwise AND of two 65536-bit bitsets (every third and every "
            "fifth bit set) into a third output bitset, also updating the "
            "output's cached cardinality via bitset_container_and(). "
            "Reported cost is per u64 word in the bitmap; inner-repeat is "
            "50 to clear the clock-resolution floor.",
            Fn([](S *s) -> int64_t {
                return bitset_container_and(s->B1, s->B2, s->BO);
            }),
            N);
        add_bench("bitset_container/and_nocard",
                  "Same as bitset_container/and but calls the _nocard variant, "
                  "which skips the popcount over the output — useful when the "
                  "caller does not need the cardinality right away.",
                  Fn([](S *s) -> int64_t {
                      bitset_container_and_nocard(s->B1, s->B2, s->BO);
                      return 0;
                  }),
                  N);
        add_bench("bitset_container/and_justcard",
                  "Computes only the cardinality of the AND of two bitsets via "
                  "bitset_container_and_justcard(), without materialising the "
                  "output container. Hot path for fast intersection-sizing.",
                  Fn([](S *s) -> int64_t {
                      return bitset_container_and_justcard(s->B1, s->B2);
                  }),
                  N);
        add_bench(
            "bitset_container/or",
            "Pointwise OR of two 65536-bit bitsets into an output bitset, "
            "updating the output cardinality. Reported cost is per u64 "
            "word in the bitmap.",
            Fn([](S *s) -> int64_t {
                return bitset_container_or(s->B1, s->B2, s->BO);
            }),
            N);
        add_bench(
            "bitset_container/or_nocard",
            "Same as bitset_container/or but skips the popcount sweep over "
            "the output via bitset_container_or_nocard().",
            Fn([](S *s) -> int64_t {
                bitset_container_or_nocard(s->B1, s->B2, s->BO);
                return 0;
            }),
            N);
        add_bench("bitset_container/or_justcard",
                  "Computes only the cardinality of the OR of two bitsets via "
                  "bitset_container_or_justcard(), without materialising the "
                  "output container. Hot path for fast union-sizing.",
                  Fn([](S *s) -> int64_t {
                      return bitset_container_or_justcard(s->B1, s->B2);
                  }),
                  N);
    }

    // bitset → array conversion.
    {
        Entry e;
        e.name = "bitset_container/to_array_convert";
        e.description =
            "Measures array_container_from_bitset() — converting a "
            "sparsely-populated 65536-bit bitset (4096 random bits set) "
            "into an array_container_t by scanning each u64 word and "
            "writing out the positions of set bits. Reported cost is per "
            "word of the source bitmap.";
        e.setup = []() -> void * {
            auto *s = new BitsetState;
            s->B = bitset_container_create();
            for (int k = 0; k < 4096; ++k) {
                bitset_container_set(
                    s->B, static_cast<uint16_t>(ranged_random(1 << 16)));
            }
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<BitsetState *>(sv);
            array_container_t *conv = array_container_from_bitset(s->B);
            int64_t card = conv->cardinality;
            array_container_free(conv);
            return card;
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<BitsetState *>(sv);
            bitset_container_free(s->B);
            delete s;
        };
        e.ops_per_run = BITSET_CONTAINER_SIZE_IN_WORDS;
        e.inner_reps = 50;
        out.push_back(std::move(e));
    }
}

// --------------------------------------------- run_container benches

struct RunState {
    run_container_t *B;
};
struct RunPairState {
    run_container_t *B1;
    run_container_t *B2;
    run_container_t *BO;
    int32_t input_total;
};

void register_run_container(std::vector<Entry> &out) {
    {
        Entry e;
        e.name = "run_container/add";
        e.description =
            "Inserts every third 16-bit value in ascending order into a "
            "freshly allocated run_container_t. Because each new value is "
            "not adjacent to the previous run, every insertion opens a "
            "brand new run, exercising the run-array extend/shift path "
            "rather than the cheap run-extension fast path. Cardinality "
            "is reported against kArrayCount.";
        e.setup = []() -> void * {
            auto *s = new RunState;
            s->B = run_container_create();
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<RunState *>(sv);
            populate_run_stride(s->B, 3);
            return run_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<RunState *>(sv);
            run_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        e.expected = kArrayCount;
        e.check_expected = true;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "run_container/contains_all_u16";
        e.description =
            "Issues 65536 membership queries against a run_container_t "
            "holding every third 16-bit value. Each query is a branchless "
            "binary search over the run-start array followed by an "
            "in-range check. Useful for comparing run-container lookup "
            "performance against array and bitset containers on the same "
            "data distribution.";
        e.setup = []() -> void * {
            auto *s = new RunState;
            s->B = run_container_create();
            populate_run_stride(s->B, 3);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<RunState *>(sv);
            int64_t card = 0;
            for (int x = 0; x < (1 << 16); ++x) {
                card += run_container_contains(s->B, static_cast<uint16_t>(x));
            }
            return card;
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<RunState *>(sv);
            run_container_free(s->B);
            delete s;
        };
        e.ops_per_run = 1 << 16;
        e.expected = kArrayCount;
        e.check_expected = true;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "run_container/remove";
        e.description =
            "Removes every stored value from a run_container_t. Each run "
            "in this benchmark is a single-element run, so removing the "
            "lone value deletes the run outright — exercising the "
            "run-array compaction path on every call. The container is "
            "repopulated before every timed iteration.";
        e.setup = []() -> void * {
            auto *s = new RunState;
            s->B = run_container_create();
            populate_run_stride(s->B, 3);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<RunState *>(sv);
            for (int x = 0; x < (1 << 16); x += 3) {
                run_container_remove(s->B, static_cast<uint16_t>(x));
            }
            return run_container_cardinality(s->B);
        };
        e.teardown = [](void *sv) {
            auto *s = static_cast<RunState *>(sv);
            run_container_free(s->B);
            delete s;
        };
        e.ops_per_run = kArrayCount;
        e.expected = 0;
        e.check_expected = true;
        out.push_back(std::move(e));
    }
    {
        struct S {
            run_container_t *B1;
            run_container_t *B2;
            run_container_t *BO;
            int32_t input_total;
        };
        auto build = []() -> void * {
            auto *s = new S;
            s->B1 = run_container_create();
            s->B2 = run_container_create();
            s->BO = run_container_create();
            populate_run_stride(s->B1, 3);
            populate_run_stride(s->B2, 5);
            s->input_total = s->B1->n_runs + s->B2->n_runs;
            return s;
        };
        auto td = [](void *sv) {
            auto *s = static_cast<S *>(sv);
            run_container_free(s->B1);
            run_container_free(s->B2);
            run_container_free(s->BO);
            delete s;
        };

        int32_t input_total = 0;
        {
            auto *probe = static_cast<S *>(build());
            input_total = probe->input_total;
            td(probe);
        }

        {
            Entry e;
            e.name = "run_container/union";
            e.description =
                "Computes the union of two run_container_t values (holding "
                "every third and every fifth 16-bit value respectively) "
                "via run_container_union(), merging their run arrays into "
                "a third output container. Reported cost is per input run "
                "(B1.n_runs + B2.n_runs); the kernel runs 200 times per "
                "timed window.";
            e.setup = build;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                run_container_union(s->B1, s->B2, s->BO);
                return run_container_cardinality(s->BO);
            };
            e.teardown = td;
            e.ops_per_run = input_total;
            e.inner_reps = 2000;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "run_container/intersection";
            e.description =
                "Computes the intersection of two run_container_t values "
                "(holding every third and every fifth 16-bit value "
                "respectively) via run_container_intersection(). Reported "
                "cost is per input run; the kernel runs 200 times per "
                "timed window.";
            e.setup = build;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                run_container_intersection(s->B1, s->B2, s->BO);
                return run_container_cardinality(s->BO);
            };
            e.teardown = td;
            e.ops_per_run = input_total;
            e.inner_reps = 2000;
            out.push_back(std::move(e));
        }
    }
}

// --------------------------------------------- equals benches

namespace equals_bench {
template <typename C1, typename C2, typename Create1, typename Create2,
          typename Free1, typename Free2, typename Add1, typename Add2,
          typename Fn>
Entry make(const char *name, const char *description, uint32_t n,
           Create1 create1, Create2 create2, Free1 free1, Free2 free2,
           Add1 add1, Add2 add2, Fn fn) {
    struct S {
        C1 *c1;
        C2 *c2;
    };
    Entry e;
    e.name = name;
    e.description = description;
    e.setup = [n, create1, create2, add1, add2]() -> void * {
        auto *s = new S;
        s->c1 = create1();
        s->c2 = create2();
        std::vector<uint16_t> vals(0x10000);
        for (uint32_t i = 0; i < 0x10000; ++i)
            vals[i] = static_cast<uint16_t>(i);
        shuffle_uint16(vals.data(), 0x10000);
        for (uint32_t i = 0; i < n; ++i) {
            add1(s->c1, vals[i]);
            add2(s->c2, vals[i]);
        }
        return s;
    };
    e.run = [fn](void *sv) -> int64_t {
        auto *s = static_cast<S *>(sv);
        return fn(s->c1, s->c2) ? 1 : 0;
    };
    e.teardown = [free1, free2](void *sv) {
        auto *s = static_cast<S *>(sv);
        free1(s->c1);
        free2(s->c2);
        delete s;
    };
    e.ops_per_run = 1;
    e.inner_reps = 50000;
    e.expected = 1;
    e.check_expected = true;
    return e;
}
}  // namespace equals_bench

void register_equals(std::vector<Entry> &out) {
    using equals_bench::make;
    auto a_create = []() { return array_container_create(); };
    auto a_free = [](array_container_t *c) { array_container_free(c); };
    auto a_add = [](array_container_t *c, uint16_t v) {
        array_container_add(c, v);
    };
    auto b_create = []() { return bitset_container_create(); };
    auto b_free = [](bitset_container_t *c) { bitset_container_free(c); };
    auto b_add = [](bitset_container_t *c, uint16_t v) {
        bitset_container_add(c, v);
    };
    auto r_create = []() { return run_container_create(); };
    auto r_free = [](run_container_t *c) { run_container_free(c); };
    auto r_add = [](run_container_t *c, uint16_t v) {
        run_container_add(c, v);
    };

    out.push_back(make<array_container_t, array_container_t>(
        "equals/array_array_n64",
        "array_container_equals() on two array_container_t values of "
        "cardinality 64, populated with the same shuffled prefix of "
        "[0, 2^16). Both sides are equal, so the comparator must walk "
        "every element before returning true. Each run is wrapped in 200 "
        "inner reps to stay above clock resolution.",
        64, a_create, a_create, a_free, a_free, a_add, a_add,
        array_container_equals));
    out.push_back(make<array_container_t, array_container_t>(
        "equals/array_array_default",
        "array_container_equals() on two equal array_container_t values "
        "of cardinality DEFAULT_MAX_SIZE (the typical array->bitset "
        "switchover boundary). Measures the equals cost at the largest "
        "size an array container is normally allowed to reach.",
        DEFAULT_MAX_SIZE, a_create, a_create, a_free, a_free, a_add, a_add,
        array_container_equals));
    out.push_back(make<array_container_t, array_container_t>(
        "equals/array_array_2xdefault",
        "array_container_equals() on two equal array_container_t values "
        "of cardinality 2 * DEFAULT_MAX_SIZE. Stresses the comparator "
        "when the container has grown past the normal conversion "
        "threshold.",
        2 * DEFAULT_MAX_SIZE, a_create, a_create, a_free, a_free, a_add, a_add,
        array_container_equals));
    out.push_back(make<bitset_container_t, bitset_container_t>(
        "equals/bitset_bitset_65535",
        "bitset_container_equals() on two equal bitsets with 65535 bits "
        "set (one bit short of full). The comparator walks every u64 "
        "word pair.",
        65535, b_create, b_create, b_free, b_free, b_add, b_add,
        bitset_container_equals));
    out.push_back(make<bitset_container_t, bitset_container_t>(
        "equals/bitset_bitset_65536",
        "bitset_container_equals() on two fully populated bitsets "
        "(65536 bits set, all-ones). Each word compares equal immediately "
        "so this hits the comparator's happy path.",
        65536, b_create, b_create, b_free, b_free, b_add, b_add,
        bitset_container_equals));
    out.push_back(make<run_container_t, run_container_t>(
        "equals/run_run_halfdefault",
        "run_container_equals() on two equal run_container_t values "
        "populated from the same shuffled prefix of size "
        "DEFAULT_MAX_SIZE/2; the resulting run arrays can vary in "
        "structure depending on which values happen to land adjacent.",
        DEFAULT_MAX_SIZE / 2, r_create, r_create, r_free, r_free, r_add, r_add,
        run_container_equals));
    out.push_back(make<run_container_t, run_container_t>(
        "equals/run_run_default",
        "run_container_equals() on two equal run_container_t values of "
        "cardinality DEFAULT_MAX_SIZE populated from the same shuffled "
        "prefix.",
        DEFAULT_MAX_SIZE, r_create, r_create, r_free, r_free, r_add, r_add,
        run_container_equals));
    out.push_back(make<run_container_t, array_container_t>(
        "equals/run_array",
        "Cross-container run_container_equals_array() comparing a "
        "run_container_t and an array_container_t that both represent "
        "the same set of values. The comparator walks runs of one side "
        "against the sorted value array of the other.",
        DEFAULT_MAX_SIZE, r_create, a_create, r_free, a_free, r_add, a_add,
        run_container_equals_array));
    out.push_back(make<array_container_t, bitset_container_t>(
        "equals/array_bitset",
        "Cross-container array_container_equal_bitset() — checks that a "
        "sorted-array container and a bitset container represent the "
        "same set by probing every array entry against the bitset and "
        "comparing cardinalities.",
        DEFAULT_MAX_SIZE, a_create, b_create, a_free, b_free, a_add, b_add,
        array_container_equal_bitset));
    out.push_back(make<run_container_t, bitset_container_t>(
        "equals/run_bitset",
        "Cross-container run_container_equals_bitset() — verifies a run "
        "container and a bitset container agree by unpacking runs into "
        "word checks against the bitset.",
        DEFAULT_MAX_SIZE, r_create, b_create, r_free, b_free, r_add, b_add,
        run_container_equals_bitset));
}

// --------------------------------------------- create benches

void register_create(std::vector<Entry> &out) {
    Entry e;
    e.name = "roaring_bitmap/create_free";
    e.description =
        "Alternates roaring_bitmap_create() with roaring_bitmap_free() on "
        "a brand-new empty bitmap. Measures the fixed per-object overhead "
        "(allocation of the bitmap struct and its empty container array), "
        "relevant anywhere an application builds many short-lived "
        "bitmaps. Inner-repeat of 10000 keeps the result well above "
        "clock resolution and amortises the kpc-read overhead.";
    e.setup = []() -> void * { return nullptr; };
    e.run = [](void *) -> int64_t {
        roaring_bitmap_t *bm = roaring_bitmap_create();
        roaring_bitmap_free(bm);
        return 1;
    };
    e.teardown = nullptr;
    e.ops_per_run = 1;
    e.inner_reps = 10000;
    out.push_back(std::move(e));
}

// --------------------------------------------- add_benchmark

namespace add_bench {

enum Order { ASC, DESC, SHUFFLE };
const char *order_name(Order o) {
    return o == ASC ? "asc" : (o == DESC ? "desc" : "shuffle");
}

struct Data {
    std::vector<uint32_t> offsets;
    uint32_t spanlen;
    uint32_t intvlen;
};

Data make_data(uint32_t spanlen, uint32_t intvlen, double density, Order o) {
    uint32_t count =
        static_cast<uint32_t>(std::floor(spanlen * density / intvlen));
    std::vector<uint32_t> offsets(count);
    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        offsets[i] = pcg32_random_r(&pcg32_global);
        sum += offsets[i];
    }
    for (uint32_t i = 0; i < count; ++i) {
        double v = offsets[i];
        v /= sum;
        v *= (spanlen - count * intvlen) / static_cast<double>(intvlen);
        uint32_t gap = static_cast<uint32_t>(std::floor(v));
        offsets[i] = (i == 0) ? gap : offsets[i - 1] + intvlen + gap;
    }
    if (o == SHUFFLE) {
        shuffle_uint32(offsets.data(), count);
    } else if (o == DESC) {
        std::reverse(offsets.begin(), offsets.end());
    }
    Data d;
    d.offsets = std::move(offsets);
    d.spanlen = spanlen;
    d.intvlen = intvlen;
    return d;
}

struct S {
    Data data;
    roaring_bitmap_t *r;
    std::vector<uint32_t> values;  // pre-expanded
};

void register_variant(std::vector<Entry> &out, const char *algo,
                      std::function<void(S *)> work, uint32_t intvlen,
                      Order order, bool pre_populated) {
    struct Wrapper {
        S s;
        std::function<void(S *)> work;
    };
    constexpr uint32_t kSpan = 1000000;
    const double kDensity = 0.2;
    Data template_data = make_data(kSpan, intvlen, kDensity, order);
    int64_t total_ops =
        static_cast<int64_t>(template_data.offsets.size()) * intvlen;

    Entry e;
    char buf[128];
    snprintf(buf, sizeof(buf), "roaring_bitmap/%s/intvlen=%u/order=%s", algo,
             intvlen, order_name(order));
    e.name = buf;

    const char *order_explanation =
        order == SHUFFLE
            ? "visited in shuffled order — offsets are Fisher-Yates "
              "permuted, so each insertion lands in an essentially "
              "random container"
            : (order == ASC
                   ? "visited in strictly ascending order — offsets are "
                     "sorted, so consecutive insertions frequently hit "
                     "the same container"
                   : "visited in strictly descending order — the worst-"
                     "case walk direction for sorted-array containers");
    const char *pop_explanation =
        pre_populated
            ? "Setup pre-populates the bitmap with the full [0, 1,000,000) "
              "range so every measured call actually removes a present "
              "value; the container mix starts as a run-container and "
              "transitions to array/bitset as it is punctured."
            : "Setup creates an empty bitmap so every measured call "
              "actually inserts a new value.";
    char descr[1024];
    snprintf(descr, sizeof(descr),
             "Measures roaring_bitmap_%s on a 20%%-occupancy 32-bit bitmap "
             "over the [0, 1,000,000) span. Data consists of %zu intervals "
             "of length %u, %s. %s Reported cost is per inserted/removed "
             "value.",
             algo, template_data.offsets.size(), intvlen, order_explanation,
             pop_explanation);
    e.description = descr;

    e.setup = [template_data, pre_populated, work]() -> void * {
        auto *w = new Wrapper;
        w->s.data = template_data;
        w->s.r = roaring_bitmap_create();
        if (pre_populated) {
            roaring_bitmap_add_range(w->s.r, 0, 1000000);
        }
        w->s.values.reserve(w->s.data.offsets.size() * w->s.data.intvlen);
        for (uint32_t off : w->s.data.offsets) {
            for (uint32_t j = 0; j < w->s.data.intvlen; ++j) {
                w->s.values.push_back(off + j);
            }
        }
        w->work = work;
        return w;
    };
    e.run = [](void *sv) -> int64_t {
        auto *w = static_cast<Wrapper *>(sv);
        w->work(&w->s);
        return 0;
    };
    e.teardown = [](void *sv) {
        auto *w = static_cast<Wrapper *>(sv);
        roaring_bitmap_free(w->s.r);
        delete w;
    };
    e.ops_per_run = total_ops;
    out.push_back(std::move(e));
}

void register_add_benchmarks(std::vector<Entry> &out) {
    // Keep the matrix modest — we pick representative intvlen × order points.
    const uint32_t intvlens[] = {1, 16};
    const Order orders[] = {SHUFFLE, ASC};
    for (Order o : orders) {
        for (uint32_t iv : intvlens) {
            register_variant(
                out, "add",
                [](S *s) {
                    for (uint32_t off : s->data.offsets) {
                        for (uint32_t j = 0; j < s->data.intvlen; ++j) {
                            roaring_bitmap_add(s->r, off + j);
                        }
                    }
                },
                iv, o, false);
            register_variant(
                out, "add_many",
                [](S *s) {
                    size_t n = s->data.offsets.size();
                    for (size_t i = 0; i < n; ++i) {
                        roaring_bitmap_add_many(
                            s->r, s->data.intvlen,
                            s->values.data() + i * s->data.intvlen);
                    }
                },
                iv, o, false);
            register_variant(
                out, "add_bulk",
                [](S *s) {
                    roaring_bulk_context_t ctx = {0, 0, 0, 0};
                    for (uint32_t off : s->data.offsets) {
                        for (uint32_t j = 0; j < s->data.intvlen; ++j) {
                            roaring_bitmap_add_bulk(s->r, &ctx, off + j);
                        }
                    }
                },
                iv, o, false);
            register_variant(
                out, "add_range",
                [](S *s) {
                    for (uint32_t off : s->data.offsets) {
                        roaring_bitmap_add_range(s->r, off,
                                                 off + s->data.intvlen);
                    }
                },
                iv, o, false);
            register_variant(
                out, "remove",
                [](S *s) {
                    for (uint32_t off : s->data.offsets) {
                        for (uint32_t j = 0; j < s->data.intvlen; ++j) {
                            roaring_bitmap_remove(s->r, off + j);
                        }
                    }
                },
                iv, o, /*pre_populated=*/true);
            register_variant(
                out, "remove_range",
                [](S *s) {
                    for (uint32_t off : s->data.offsets) {
                        roaring_bitmap_remove_range(s->r, off,
                                                    off + s->data.intvlen);
                    }
                },
                iv, o, /*pre_populated=*/true);
        }
    }
}
}  // namespace add_bench

// --------------------------------------------- adversarialunions

namespace adversarial {

struct Many {
    std::vector<roaring_bitmap_t *> bitmaps;
};

Many *build(int stride_gap) {
    constexpr size_t bitmapcount = 100;
    constexpr size_t size = 1000000;
    auto *m = new Many;
    m->bitmaps.reserve(bitmapcount);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> d(0, size - 1);
    for (size_t i = 0; i < bitmapcount; ++i) {
        roaring_bitmap_t *b = roaring_bitmap_from_range(
            0, size, stride_gap == 0 ? 1 : stride_gap);
        for (size_t j = 0; j < size / 20; ++j) {
            roaring_bitmap_remove(b, d(rng));
        }
        roaring_bitmap_run_optimize(b);
        m->bitmaps.push_back(b);
    }
    return m;
}

void free_many(void *sv) {
    auto *m = static_cast<Many *>(sv);
    for (auto *b : m->bitmaps) roaring_bitmap_free(b);
    delete m;
}

void register_benchmarks(std::vector<Entry> &out) {
    using Fn = std::function<int64_t(Many *)>;
    auto add = [&](const char *name, const char *descr, int stride_gap, Fn fn) {
        Entry e;
        e.name = name;
        e.description = descr;
        e.setup = [stride_gap]() -> void * { return build(stride_gap); };
        e.run = [fn](void *sv) -> int64_t {
            auto *m = static_cast<Many *>(sv);
            return fn(m);
        };
        e.teardown = free_many;
        e.ops_per_run = 100;  // number of bitmaps unioned
        // Building 100 × 1M-range bitmaps is expensive and the workload does
        // not mutate them — share across iterations.
        e.reusable_state = true;
        out.push_back(std::move(e));
    };

    const char *quickfull_setup =
        "Inputs are 100 bitmaps built from roaring_bitmap_from_range(0, "
        "1,000,000, 1) — initially every value in [0, 1M) — with 5% of "
        "values subsequently removed at random positions and then "
        "run_optimize() applied. Each individual bitmap is dense, so a "
        "pairwise union fills up very quickly: the 'quickfull' regime "
        "where the naive accumulator converges to a full bitmap after "
        "just a handful of OR operations. ";
    const char *notsofull_setup =
        "Inputs are 100 bitmaps built from roaring_bitmap_from_range(0, "
        "1,000,000, 100) — only every hundredth value set — with 5% of "
        "those values then removed. Each individual bitmap is sparse and "
        "they share few elements, so the accumulator stays sparse for "
        "many unions: the 'notsofull' regime where the naive approach "
        "performs many expensive sparse-to-sparse merges. ";

    add("adversarial/quickfull/or_many_heap",
        (std::string(quickfull_setup) +
         "This variant uses roaring_bitmap_or_many_heap, which performs a "
         "pairwise tournament union via a priority queue — generally the "
         "best strategy on very sparse inputs, less so here.")
            .c_str(),
        1, Fn([](Many *m) -> int64_t {
            roaring_bitmap_t *ans = roaring_bitmap_or_many_heap(
                m->bitmaps.size(),
                const_cast<const roaring_bitmap_t **>(m->bitmaps.data()));
            int64_t card = roaring_bitmap_get_cardinality(ans);
            roaring_bitmap_free(ans);
            return card;
        }));
    add("adversarial/quickfull/or_many",
        (std::string(quickfull_setup) +
         "This variant uses roaring_bitmap_or_many, the in-place N-way "
         "union that streams inputs into a single accumulator and should "
         "win on quickly-saturating data.")
            .c_str(),
        1, Fn([](Many *m) -> int64_t {
            roaring_bitmap_t *ans = roaring_bitmap_or_many(
                m->bitmaps.size(),
                const_cast<const roaring_bitmap_t **>(m->bitmaps.data()));
            int64_t card = roaring_bitmap_get_cardinality(ans);
            roaring_bitmap_free(ans);
            return card;
        }));
    add("adversarial/quickfull/or_inplace_naive",
        (std::string(quickfull_setup) +
         "This variant uses the naive reduce: copy bitmap[0] and call "
         "roaring_bitmap_or_inplace in a loop. On quickfull data the "
         "accumulator becomes full fast so every subsequent OR is cheap, "
         "often beating the dedicated many-ways.")
            .c_str(),
        1, Fn([](Many *m) -> int64_t {
            roaring_bitmap_t *ans = roaring_bitmap_copy(m->bitmaps[0]);
            for (size_t i = 1; i < m->bitmaps.size(); ++i) {
                roaring_bitmap_or_inplace(ans, m->bitmaps[i]);
            }
            int64_t card = roaring_bitmap_get_cardinality(ans);
            roaring_bitmap_free(ans);
            return card;
        }));
    add("adversarial/notsofull/or_many_heap",
        (std::string(notsofull_setup) +
         "This variant uses roaring_bitmap_or_many_heap, the priority-"
         "queue pairwise tournament. Tends to win here because merges "
         "stay small.")
            .c_str(),
        100, Fn([](Many *m) -> int64_t {
            roaring_bitmap_t *ans = roaring_bitmap_or_many_heap(
                m->bitmaps.size(),
                const_cast<const roaring_bitmap_t **>(m->bitmaps.data()));
            int64_t card = roaring_bitmap_get_cardinality(ans);
            roaring_bitmap_free(ans);
            return card;
        }));
    add("adversarial/notsofull/or_many",
        (std::string(notsofull_setup) +
         "This variant uses roaring_bitmap_or_many, the in-place N-way "
         "union.")
            .c_str(),
        100, Fn([](Many *m) -> int64_t {
            roaring_bitmap_t *ans = roaring_bitmap_or_many(
                m->bitmaps.size(),
                const_cast<const roaring_bitmap_t **>(m->bitmaps.data()));
            int64_t card = roaring_bitmap_get_cardinality(ans);
            roaring_bitmap_free(ans);
            return card;
        }));
    add("adversarial/notsofull/or_inplace_naive",
        (std::string(notsofull_setup) +
         "This variant uses the naive reduce with roaring_bitmap_or_inplace "
         "in a loop. Typically loses on notsofull data because the "
         "accumulator stays sparse and every merge is expensive.")
            .c_str(),
        100, Fn([](Many *m) -> int64_t {
            roaring_bitmap_t *ans = roaring_bitmap_copy(m->bitmaps[0]);
            for (size_t i = 1; i < m->bitmaps.size(); ++i) {
                roaring_bitmap_or_inplace(ans, m->bitmaps[i]);
            }
            int64_t card = roaring_bitmap_get_cardinality(ans);
            roaring_bitmap_free(ans);
            return card;
        }));
}
}  // namespace adversarial

// --------------------------------------------- intersect_range

namespace intersect_range {

struct TestValue {
    roaring_bitmap_t *bitmap;
    uint32_t rstart;
    uint32_t rstop;
    bool expected;
};

uint32_t rand_exclusive(uint32_t max_plus_one) {
    // ranged_random returns [0, range). We want [0, max_plus_one - 1].
    return ranged_random(max_plus_one);
}

void register_benchmarks(std::vector<Entry> &out) {
    struct S {
        std::vector<TestValue> tv;
    };
    auto setup = []() -> void * {
        auto *s = new S;
        s->tv.reserve(100);
        for (int i = 0; i < 100; ++i) {
            uint32_t a = rand_exclusive((1u << 31));
            uint32_t b = rand_exclusive((1u << 31));
            if (a > b) std::swap(a, b);
            if (a == b) ++b;
            uint32_t step = ranged_random((1u << 16) - 1);
            if (step == 0) step = 1;
            TestValue t;
            t.bitmap = roaring_bitmap_from_range(a, b, step);
            // Sample a sub-range of that span for the intersection test.
            uint32_t c = rand_exclusive((1u << 31));
            uint32_t d = rand_exclusive((1u << 31));
            if (c > d) std::swap(c, d);
            if (c == d) ++d;
            t.rstart = c;
            t.rstop = d;
            // Expected answer: naive intersection via from_range + intersect.
            roaring_bitmap_t *rng = roaring_bitmap_from_range(c, d, 1);
            t.expected = roaring_bitmap_intersect(t.bitmap, rng);
            roaring_bitmap_free(rng);
            s->tv.push_back(t);
        }
        return s;
    };
    auto td = [](void *sv) {
        auto *s = static_cast<S *>(sv);
        for (auto &t : s->tv) roaring_bitmap_free(t.bitmap);
        delete s;
    };

    {
        Entry e;
        e.name = "intersect_range/naive_from_range";
        e.description =
            "For each of 100 random (bitmap, range) test cases, answers "
            "\"does bitmap intersect [rstart, rstop)?\" by first "
            "materialising the range as a full roaring_bitmap via "
            "roaring_bitmap_from_range() and then calling "
            "roaring_bitmap_intersect() on it. This is the naive strategy "
            "— it allocates an intermediate bitmap on every call — and "
            "serves as the baseline the dedicated API must beat. The 100 "
            "bitmaps are built once per benchmark run and shared across "
            "measured iterations.";
        e.setup = setup;
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            int64_t n = 0;
            for (auto &t : s->tv) {
                roaring_bitmap_t *r =
                    roaring_bitmap_from_range(t.rstart, t.rstop, 1);
                n += roaring_bitmap_intersect(t.bitmap, r) ? 1 : 0;
                roaring_bitmap_free(r);
            }
            return n;
        };
        e.teardown = td;
        e.ops_per_run = 100;
        e.inner_reps = 50;
        e.reusable_state = true;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "intersect_range/intersect_with_range";
        e.description =
            "For each of 100 random (bitmap, range) test cases, answers "
            "the same \"does bitmap intersect [rstart, rstop)?\" question "
            "via the dedicated roaring_bitmap_intersect_with_range() "
            "entry point — no allocation, container-by-container check. "
            "A poison subtraction inside run() detects any case where the "
            "dedicated API disagrees with the naive baseline (guards "
            "against silent correctness regressions).";
        e.setup = setup;
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            int64_t n = 0;
            for (auto &t : s->tv) {
                bool r = roaring_bitmap_intersect_with_range(t.bitmap, t.rstart,
                                                             t.rstop);
                n += r ? 1 : 0;
                if (r != t.expected) n -= 1000000000;  // poison for filter
            }
            return n;
        };
        e.teardown = td;
        e.ops_per_run = 100;
        e.inner_reps = 500;
        e.reusable_state = true;
        out.push_back(std::move(e));
    }
}
}  // namespace intersect_range

// --------------------------------------------- data-directory benches

struct LoadedBitmaps {
    std::vector<roaring_bitmap_t *> bitmaps;
    // 64-bit variants (C and C++ wrappers) built from the same inputs
    // so microbench/bench.cpp-equivalent benchmarks can hit the exact
    // same APIs as the standalone bench.cpp executable.
    std::vector<roaring64_bitmap_t *> bitmaps64;
    std::vector<Roaring64Map *> bitmaps64cpp;
    std::vector<uint32_t> maxvals;
    std::vector<uint64_t> cards;
    std::vector<std::vector<uint32_t>> raw;  // original integer inputs, kept
                                             // so we can shuffle & re-query
    roaring_bitmap_t *merged;                // union of everything
    uint32_t global_max = 0;                 // max value across all bitmaps
    uint32_t max_card = 0;  // max cardinality of any individual bitmap
    uint32_t *array_buffer = nullptr;    // sized to max_card (uint32_t)
    uint64_t *array_buffer64 = nullptr;  // sized to max_card (uint64_t)
};

LoadedBitmaps *load_directory(const char *dirname, const char *ext) {
    size_t count = 0;
    size_t *howmany = nullptr;
    uint32_t **numbers = read_all_integer_files(dirname, ext, &howmany, &count);
    if (!numbers || count == 0) {
        free(howmany);
        free(numbers);
        return nullptr;
    }
    auto *loaded = new LoadedBitmaps;
    loaded->bitmaps.reserve(count);
    loaded->bitmaps64.reserve(count);
    loaded->bitmaps64cpp.reserve(count);
    loaded->maxvals.reserve(count);
    loaded->cards.reserve(count);
    loaded->raw.resize(count);
    loaded->merged = roaring_bitmap_create();
    for (size_t i = 0; i < count; ++i) {
        roaring_bitmap_t *b = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        roaring_bitmap_run_optimize(b);
        roaring_bitmap_shrink_to_fit(b);
        loaded->bitmaps.push_back(b);

        roaring64_bitmap_t *b64 = roaring64_bitmap_create();
        auto *bcpp = new Roaring64Map();
        for (size_t j = 0; j < howmany[i]; ++j) {
            roaring64_bitmap_add(b64, numbers[i][j]);
            bcpp->add(numbers[i][j]);
        }
        roaring64_bitmap_run_optimize(b64);
        bcpp->runOptimize();
        loaded->bitmaps64.push_back(b64);
        loaded->bitmaps64cpp.push_back(bcpp);

        uint32_t m = roaring_bitmap_is_empty(b) ? 0 : roaring_bitmap_maximum(b);
        loaded->maxvals.push_back(m);
        if (m > loaded->global_max) loaded->global_max = m;
        if (howmany[i] > loaded->max_card)
            loaded->max_card = static_cast<uint32_t>(howmany[i]);
        loaded->cards.push_back(roaring_bitmap_get_cardinality(b));
        loaded->raw[i].assign(numbers[i], numbers[i] + howmany[i]);
        roaring_bitmap_or_inplace(loaded->merged, b);
        free(numbers[i]);
    }
    free(numbers);
    free(howmany);
    loaded->array_buffer = static_cast<uint32_t *>(
        malloc(sizeof(uint32_t) * std::max<uint32_t>(1, loaded->max_card)));
    loaded->array_buffer64 = static_cast<uint64_t *>(
        malloc(sizeof(uint64_t) * std::max<uint32_t>(1, loaded->max_card)));
    return loaded;
}

void free_loaded(LoadedBitmaps *loaded) {
    for (auto *b : loaded->bitmaps) roaring_bitmap_free(b);
    for (auto *b : loaded->bitmaps64) roaring64_bitmap_free(b);
    for (auto *b : loaded->bitmaps64cpp) delete b;
    roaring_bitmap_free(loaded->merged);
    free(loaded->array_buffer);
    free(loaded->array_buffer64);
    delete loaded;
}

// Register the real-bitmap benchmark family against a single dataset. Each
// registered benchmark name is tagged with the dataset (e.g.
// "real_bitmaps/contains_quartiles/census-income") so filters can select
// either a family across datasets (`--filter contains_quartiles`) or a
// dataset across families (`--filter census-income`).
void register_real_bitmaps(std::vector<Entry> &out, LoadedBitmaps *loaded,
                           const std::string &dataset) {
    if (!loaded || loaded->bitmaps.empty()) return;
    const std::string suffix = "/" + dataset;
    char countbuf[64];
    snprintf(countbuf, sizeof(countbuf), "%zu", loaded->bitmaps.size());
    const std::string N_bitmaps = countbuf;
    const std::string in_dataset =
        " (dataset \"" + dataset + "\", " + N_bitmaps + " bitmaps)";

    // Per-bitmap quartile queries: each bitmap uses ITS OWN maximum, so
    // probes stay in range. Old code used bitmaps[0]'s max for everything.
    {
        Entry e;
        e.name = "real_bitmaps/contains_quartiles" + suffix;
        e.description =
            "For every bitmap in the \"" + dataset + "\" dataset (" +
            N_bitmaps +
            " bitmaps total), issues three membership queries at 1/4, "
            "1/2, and 3/4 of THAT bitmap's own maximum value. The old "
            "real_bitmaps_contains benchmark computed a single global "
            "maximum and queried every bitmap at the same three absolute "
            "values, which typically landed far above most bitmaps' "
            "actual ranges and thus measured a trivial early-out path; "
            "this version keeps queries in-range for each bitmap. The "
            "bitmaps themselves are shared across iterations.";
        e.setup = [loaded]() -> void * { return loaded; };
        e.run = [](void *sv) -> int64_t {
            auto *lb = static_cast<LoadedBitmaps *>(sv);
            int64_t hits = 0;
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                uint32_t m = lb->maxvals[i];
                hits += roaring_bitmap_contains(lb->bitmaps[i], m / 4);
                hits += roaring_bitmap_contains(lb->bitmaps[i], m / 2);
                hits += roaring_bitmap_contains(lb->bitmaps[i], 3 * m / 4);
            }
            return hits;
        };
        e.teardown = nullptr;
        e.ops_per_run = 3 * static_cast<int64_t>(loaded->bitmaps.size());
        e.inner_reps = 200;
        out.push_back(std::move(e));
    }

    // Successive AND / OR between adjacent bitmaps (from
    // real_bitmaps_benchmark).
    {
        Entry e;
        e.name = "real_bitmaps/successive_and" + suffix;
        e.description =
            "Walks the bitmaps of the \"" + dataset +
            "\" dataset and computes roaring_bitmap_and(bitmap[i], "
            "bitmap[i+1]) for every adjacent pair, summing the "
            "cardinalities of the resulting intersections. Measures "
            "sustained intersection throughput on real data; reported "
            "cost is per adjacent pair. The input bitmaps are shared "
            "across iterations and never mutated." +
            in_dataset;
        e.setup = [loaded]() -> void * { return loaded; };
        e.run = [](void *sv) -> int64_t {
            auto *lb = static_cast<LoadedBitmaps *>(sv);
            int64_t sum = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                roaring_bitmap_t *r =
                    roaring_bitmap_and(lb->bitmaps[i], lb->bitmaps[i + 1]);
                sum += roaring_bitmap_get_cardinality(r);
                roaring_bitmap_free(r);
            }
            return sum;
        };
        e.teardown = nullptr;
        e.ops_per_run = std::max<size_t>(1, loaded->bitmaps.size() - 1);
        e.inner_reps = 20;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "real_bitmaps/successive_or" + suffix;
        e.description =
            "Walks the bitmaps of the \"" + dataset +
            "\" dataset and computes roaring_bitmap_or(bitmap[i], "
            "bitmap[i+1]) for every adjacent pair, summing the "
            "cardinalities of the resulting unions. Measures sustained "
            "union throughput on real data; reported cost is per adjacent "
            "pair. Inputs are shared across iterations." +
            in_dataset;
        e.setup = [loaded]() -> void * { return loaded; };
        e.run = [](void *sv) -> int64_t {
            auto *lb = static_cast<LoadedBitmaps *>(sv);
            int64_t sum = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                roaring_bitmap_t *r =
                    roaring_bitmap_or(lb->bitmaps[i], lb->bitmaps[i + 1]);
                sum += roaring_bitmap_get_cardinality(r);
                roaring_bitmap_free(r);
            }
            return sum;
        };
        e.teardown = nullptr;
        e.ops_per_run = std::max<size_t>(1, loaded->bitmaps.size() - 1);
        e.inner_reps = 20;
        out.push_back(std::move(e));
    }

    // Full-iteration walks over the merged bitmap (from iteration_benchmark).
    {
        struct S {
            roaring_bitmap_t *bm;
            uint64_t card;
        };
        Entry e;
        e.name = "iteration/advance" + suffix;
        e.description =
            "Iterates over every value in the union (merged bitmap) of "
            "all bitmaps in the \"" +
            dataset +
            "\" dataset using roaring_uint32_iterator_advance() — the "
            "one-value-at-a-time API. Accumulates a sum of visited "
            "values so the compiler cannot elide the loop. Reported cost "
            "is per iterated value." +
            in_dataset;
        e.setup = [loaded]() -> void * {
            auto *s = new S;
            s->bm = loaded->merged;
            s->card = roaring_bitmap_get_cardinality(loaded->merged);
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            roaring_uint32_iterator_t it;
            roaring_iterator_init(s->bm, &it);
            uint64_t sum = 0;
            while (it.has_value) {
                sum += it.current_value;
                roaring_uint32_iterator_advance(&it);
            }
            return static_cast<int64_t>(sum);
        };
        e.teardown = [](void *sv) { delete static_cast<S *>(sv); };
        e.ops_per_run = static_cast<int64_t>(
            roaring_bitmap_get_cardinality(loaded->merged));
        e.inner_reps = 20;
        out.push_back(std::move(e));
    }
    const uint32_t bufsizes[] = {1, 4, 16, 128, 1024};
    for (uint32_t bs : bufsizes) {
        struct S {
            roaring_bitmap_t *bm;
            uint32_t bufsize;
        };
        Entry e;
        char buf[96];
        snprintf(buf, sizeof(buf), "iteration/read/bufsize=%u/%s", bs,
                 dataset.c_str());
        e.name = buf;
        char dbuf[768];
        snprintf(dbuf, sizeof(dbuf),
                 "Iterates over every value in the merged bitmap (union "
                 "of all bitmaps in the \"%s\" dataset) using "
                 "roaring_uint32_iterator_read() with a %u-element "
                 "buffer. Each call drains up to %u values, amortising "
                 "the iterator state machine over multiple values. "
                 "Comparing the bufsize=1 variant against "
                 "iteration/advance shows the per-call overhead of "
                 "advance(); comparing larger bufsizes shows where the "
                 "amortisation plateaus. Reported cost is per iterated "
                 "value.%s",
                 dataset.c_str(), bs, bs, in_dataset.c_str());
        e.description = dbuf;
        e.setup = [loaded, bs]() -> void * {
            auto *s = new S;
            s->bm = loaded->merged;
            s->bufsize = bs;
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            std::vector<uint32_t> buffer(s->bufsize);
            roaring_uint32_iterator_t it;
            roaring_iterator_init(s->bm, &it);
            uint64_t sum = 0;
            while (true) {
                uint32_t got = roaring_uint32_iterator_read(&it, buffer.data(),
                                                            s->bufsize);
                for (uint32_t i = 0; i < got; ++i) sum += buffer[i];
                if (got < s->bufsize) break;
            }
            return static_cast<int64_t>(sum);
        };
        e.teardown = [](void *sv) { delete static_cast<S *>(sv); };
        e.ops_per_run = static_cast<int64_t>(
            roaring_bitmap_get_cardinality(loaded->merged));
        e.inner_reps = 20;
        out.push_back(std::move(e));
    }

    // Contains multi (containsmulti_benchmark): query whole input sets.
    {
        struct S {
            LoadedBitmaps *lb;
            std::vector<uint32_t> queries;  // shuffled across all files
            std::vector<uint32_t> queries_sorted;
        };
        auto make_setup = [loaded]() {
            return [loaded]() -> void * {
                auto *s = new S;
                s->lb = loaded;
                for (auto &v : loaded->raw) {
                    s->queries.insert(s->queries.end(), v.begin(), v.end());
                }
                shuffle_uint32(s->queries.data(),
                               static_cast<uint32_t>(s->queries.size()));
                s->queries_sorted = s->queries;
                std::sort(s->queries_sorted.begin(), s->queries_sorted.end());
                return s;
            };
        };
        auto td = [](void *sv) { delete static_cast<S *>(sv); };

        int64_t total_card = static_cast<int64_t>(std::accumulate(
            loaded->cards.begin(), loaded->cards.end(), uint64_t{0}));
        {
            Entry e;
            e.name = "contains_multi/contains_shuffled" + suffix;
            e.description =
                "Queries every value from every input file of the \"" +
                dataset +
                "\" dataset (concatenated and Fisher-Yates shuffled) "
                "against the merged bitmap using roaring_bitmap_contains() "
                "— one independent lookup per value. Shuffling defeats "
                "spatial locality, so this is the worst case for Roaring's "
                "container cache; reported cost is per query." +
                in_dataset;
            e.setup = make_setup();
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                for (uint32_t q : s->queries) {
                    hits += roaring_bitmap_contains(s->lb->merged, q);
                }
                return hits;
            };
            e.teardown = td;
            e.ops_per_run = total_card;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "contains_multi/contains_bulk_shuffled" + suffix;
            e.description =
                "Same workload as contains_multi/contains_shuffled on the "
                "\"" +
                dataset +
                "\" dataset (shuffled queries of every input value "
                "against the merged bitmap) but via "
                "roaring_bitmap_contains_bulk(), which carries a "
                "roaring_bulk_context_t across calls to skip the "
                "high-word lookup when consecutive queries hit the same "
                "container. With shuffled input the context rarely helps; "
                "this is the comparison point." +
                in_dataset;
            e.setup = make_setup();
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                roaring_bulk_context_t ctx = {0, 0, 0, 0};
                for (uint32_t q : s->queries) {
                    hits +=
                        roaring_bitmap_contains_bulk(s->lb->merged, &ctx, q);
                }
                return hits;
            };
            e.teardown = td;
            e.ops_per_run = total_card;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "contains_multi/contains_sorted" + suffix;
            e.description =
                "Queries every value from every input file of the \"" +
                dataset +
                "\" dataset (concatenated and std::sorted) against the "
                "merged bitmap using the plain roaring_bitmap_contains(). "
                "Sorted input maximises container-locality; this is the "
                "best-case for the non-bulk API and the comparison point "
                "for contains_bulk_sorted." +
                in_dataset;
            e.setup = make_setup();
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                for (uint32_t q : s->queries_sorted) {
                    hits += roaring_bitmap_contains(s->lb->merged, q);
                }
                return hits;
            };
            e.teardown = td;
            e.ops_per_run = total_card;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "contains_multi/contains_bulk_sorted" + suffix;
            e.description =
                "Queries every value from every input file of the \"" +
                dataset +
                "\" dataset (sorted) against the merged bitmap via "
                "roaring_bitmap_contains_bulk(). Because consecutive "
                "queries hit the same high-word (and therefore the same "
                "container) in bursts, the bulk context pays off — this "
                "should be the fastest of the four contains_multi "
                "variants." +
                in_dataset;
            e.setup = make_setup();
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t hits = 0;
                roaring_bulk_context_t ctx = {0, 0, 0, 0};
                for (uint32_t q : s->queries_sorted) {
                    hits +=
                        roaring_bitmap_contains_bulk(s->lb->merged, &ctx, q);
                }
                return hits;
            };
            e.teardown = td;
            e.ops_per_run = total_card;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
    }

    // Frozen serialize / view round-trips for each bitmap in the set.
    {
        struct S {
            LoadedBitmaps *lb;
            std::vector<std::vector<char>> portable_bufs;
            std::vector<std::pair<char *, size_t>> frozen_bufs;  // aligned
        };
        auto setup = [loaded]() -> void * {
            auto *s = new S;
            s->lb = loaded;
            s->portable_bufs.reserve(loaded->bitmaps.size());
            s->frozen_bufs.reserve(loaded->bitmaps.size());
            for (auto *b : loaded->bitmaps) {
                size_t psize = roaring_bitmap_portable_size_in_bytes(b);
                std::vector<char> pbuf(psize);
                roaring_bitmap_portable_serialize(b, pbuf.data());
                s->portable_bufs.push_back(std::move(pbuf));
                size_t fsize = roaring_bitmap_frozen_size_in_bytes(b);
                char *fbuf =
                    static_cast<char *>(roaring_aligned_malloc(32, fsize));
                roaring_bitmap_frozen_serialize(b, fbuf);
                s->frozen_bufs.emplace_back(fbuf, fsize);
            }
            return s;
        };
        auto td = [](void *sv) {
            auto *s = static_cast<S *>(sv);
            for (auto &p : s->frozen_bufs) roaring_aligned_free(p.first);
            delete s;
        };

        {
            Entry e;
            e.name = "frozen/portable_deserialize" + suffix;
            e.description =
                "For every bitmap in the \"" + dataset +
                "\" dataset, walks the portable serialization buffer "
                "(produced once in setup via "
                "roaring_bitmap_portable_serialize) and rebuilds a full "
                "roaring_bitmap_t via roaring_bitmap_portable_deserialize. "
                "This is the general-purpose round-trip: allocates "
                "containers, copies all run/array/bitset contents." +
                in_dataset;
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t sum = 0;
                for (auto &buf : s->portable_bufs) {
                    roaring_bitmap_t *b =
                        roaring_bitmap_portable_deserialize(buf.data());
                    sum += roaring_bitmap_get_cardinality(b);
                    roaring_bitmap_free(b);
                }
                return sum;
            };
            e.teardown = td;
            e.ops_per_run = static_cast<int64_t>(loaded->bitmaps.size());
            e.inner_reps = 50;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "frozen/portable_deserialize_frozen" + suffix;
            e.description =
                "For every bitmap in the \"" + dataset +
                "\" dataset, walks the portable serialization buffer and "
                "calls roaring_bitmap_portable_deserialize_frozen, which "
                "produces a bitmap whose containers point directly into "
                "the source buffer. Substantially faster than the full "
                "deserialize because no container data is copied, but the "
                "resulting bitmap is read-only and tied to the buffer's "
                "lifetime." +
                in_dataset;
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t sum = 0;
                for (auto &buf : s->portable_bufs) {
                    roaring_bitmap_t *b =
                        roaring_bitmap_portable_deserialize_frozen(buf.data());
                    sum += roaring_bitmap_get_cardinality(b);
                    roaring_bitmap_free(b);
                }
                return sum;
            };
            e.teardown = td;
            e.ops_per_run = static_cast<int64_t>(loaded->bitmaps.size());
            e.inner_reps = 50;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = "frozen/frozen_view" + suffix;
            e.description =
                "For every bitmap in the \"" + dataset +
                "\" dataset, constructs a read-only bitmap view over a "
                "pre-serialized native frozen buffer via "
                "roaring_bitmap_frozen_view(). The native frozen format "
                "is not portable across machines but the view is created "
                "without any parsing work — essentially just pointer "
                "arithmetic over the 32-byte aligned buffer." +
                in_dataset;
            e.setup = setup;
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<S *>(sv);
                int64_t sum = 0;
                for (auto &p : s->frozen_bufs) {
                    const roaring_bitmap_t *b =
                        roaring_bitmap_frozen_view(p.first, p.second);
                    sum += roaring_bitmap_get_cardinality(b);
                    roaring_bitmap_free(b);
                }
                return sum;
            };
            e.teardown = td;
            e.ops_per_run = static_cast<int64_t>(loaded->bitmaps.size());
            e.inner_reps = 50;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
    }
}

// Scan a directory for immediate subdirectories that contain at least one
// file with the given extension. Returns a sorted list of subdirectory
// names (not full paths). Used to auto-discover every dataset under the
// build-time realdata/ directory.
std::vector<std::string> discover_datasets(const char *parent,
                                           const char *ext) {
    std::vector<std::string> out;
    DIR *d = opendir(parent);
    if (!d) return out;
    struct dirent *entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_name[0] == '.') continue;  // skip . and ..
        std::string path = std::string(parent) + "/" + entry->d_name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        // Check the subdir contains at least one file with `ext`.
        DIR *sd = opendir(path.c_str());
        if (!sd) continue;
        bool has_data = false;
        while (struct dirent *se = readdir(sd)) {
            const char *dot = strrchr(se->d_name, '.');
            if (dot && strcmp(dot, ext) == 0) {
                has_data = true;
                break;
            }
        }
        closedir(sd);
        if (has_data) out.emplace_back(entry->d_name);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

// ------------------------------------------- bench.cpp equivalents
//
// Register a one-to-one port of every benchmark in
// microbenchmarks/bench.cpp. Each entry runs on the same data a given
// dataset provides, so numbers are directly comparable with the
// standalone bench.cpp executable that ships with CRoaring. Benchmark
// names preserve the bench.cpp CamelCase so you can grep for them:
//
//   microbench/SuccessiveIntersection/census-income
//   microbench/RandomAccess64Cpp/weather_sept_85
//   ...
//
// All benchmarks here treat the loaded bitmaps as read-only and share
// state across iterations (reusable_state = true).

namespace microbench {

void register_all(std::vector<Entry> &out, LoadedBitmaps *loaded,
                  const std::string &dataset) {
    if (!loaded || loaded->bitmaps.empty()) return;
    const std::string suffix = "/" + dataset;
    const int64_t count = static_cast<int64_t>(loaded->bitmaps.size());
    const int64_t pairs = std::max<int64_t>(1, count - 1);

    auto add_bench = [&](const char *name, const char *description,
                         std::function<int64_t(LoadedBitmaps *)> fn,
                         int64_t ops_per_run, int inner_reps) {
        Entry e;
        e.name = std::string("microbench/") + name + suffix;
        e.description = description;
        e.setup = [loaded]() -> void * { return loaded; };
        e.run = [fn](void *sv) -> int64_t {
            return fn(static_cast<LoadedBitmaps *>(sv));
        };
        e.teardown = nullptr;
        e.ops_per_run = ops_per_run;
        e.inner_reps = inner_reps;
        e.reusable_state = true;
        out.push_back(std::move(e));
    };

    // ---- Successive intersections / unions -----------------------------
    add_bench(
        "SuccessiveIntersection",
        "bench.cpp SuccessiveIntersection: for every adjacent pair of 32-bit "
        "bitmaps in the dataset, computes roaring_bitmap_and(), sums the "
        "cardinality of the intersection, then frees it. Measures "
        "full-materialisation AND throughput on real data.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                roaring_bitmap_t *t =
                    roaring_bitmap_and(lb->bitmaps[i], lb->bitmaps[i + 1]);
                marker += roaring_bitmap_get_cardinality(t);
                roaring_bitmap_free(t);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 20);

    add_bench(
        "SuccessiveIntersection64",
        "bench.cpp SuccessiveIntersection64: same as SuccessiveIntersection "
        "but on the roaring64_bitmap_t variants built from the same "
        "inputs. Exercises the 64-bit intersection path including the "
        "art-tree key traversal.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps64.size(); ++i) {
                roaring64_bitmap_t *t = roaring64_bitmap_and(
                    lb->bitmaps64[i], lb->bitmaps64[i + 1]);
                marker += roaring64_bitmap_get_cardinality(t);
                roaring64_bitmap_free(t);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 10);

    add_bench(
        "SuccessiveIntersectionCardinality",
        "bench.cpp SuccessiveIntersectionCardinality: for every adjacent "
        "pair, computes roaring_bitmap_and_cardinality() directly, without "
        "materialising the output bitmap. Typically far faster than "
        "SuccessiveIntersection because no allocation is required.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                marker += roaring_bitmap_and_cardinality(lb->bitmaps[i],
                                                         lb->bitmaps[i + 1]);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 50);

    add_bench(
        "SuccessiveIntersectionCardinality64",
        "bench.cpp SuccessiveIntersectionCardinality64: 64-bit version of "
        "SuccessiveIntersectionCardinality — calls "
        "roaring64_bitmap_and_cardinality() on adjacent pairs.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps64.size(); ++i) {
                marker += roaring64_bitmap_and_cardinality(
                    lb->bitmaps64[i], lb->bitmaps64[i + 1]);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 50);

    add_bench(
        "SuccessiveUnion",
        "bench.cpp SuccessiveUnion: for every adjacent pair of 32-bit "
        "bitmaps, computes roaring_bitmap_or(), sums the cardinality of "
        "the union, then frees it. Measures full-materialisation OR "
        "throughput on real data.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                roaring_bitmap_t *t =
                    roaring_bitmap_or(lb->bitmaps[i], lb->bitmaps[i + 1]);
                marker += roaring_bitmap_get_cardinality(t);
                roaring_bitmap_free(t);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 20);

    add_bench(
        "SuccessiveUnion64",
        "bench.cpp SuccessiveUnion64: 64-bit adjacent-pair OR.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps64.size(); ++i) {
                roaring64_bitmap_t *t =
                    roaring64_bitmap_or(lb->bitmaps64[i], lb->bitmaps64[i + 1]);
                marker += roaring64_bitmap_get_cardinality(t);
                roaring64_bitmap_free(t);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 10);

    add_bench(
        "SuccessiveUnionCardinality",
        "bench.cpp SuccessiveUnionCardinality: adjacent-pair "
        "roaring_bitmap_or_cardinality() — computes union cardinality "
        "without materialising the union bitmap.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                marker += roaring_bitmap_or_cardinality(lb->bitmaps[i],
                                                        lb->bitmaps[i + 1]);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 50);

    add_bench(
        "SuccessiveUnionCardinality64",
        "bench.cpp SuccessiveUnionCardinality64: 64-bit adjacent-pair "
        "roaring64_bitmap_or_cardinality().",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps64.size(); ++i) {
                marker += roaring64_bitmap_or_cardinality(lb->bitmaps64[i],
                                                          lb->bitmaps64[i + 1]);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 50);

    add_bench(
        "SuccessiveDifferenceCardinality",
        "bench.cpp SuccessiveDifferenceCardinality: adjacent-pair "
        "roaring_bitmap_andnot_cardinality() — computes |A \\ B| without "
        "materialising the difference.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps.size(); ++i) {
                marker += roaring_bitmap_andnot_cardinality(lb->bitmaps[i],
                                                            lb->bitmaps[i + 1]);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 50);

    add_bench(
        "SuccessiveDifferenceCardinality64",
        "bench.cpp SuccessiveDifferenceCardinality64: 64-bit "
        "roaring64_bitmap_andnot_cardinality() on adjacent pairs.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i + 1 < lb->bitmaps64.size(); ++i) {
                marker += roaring64_bitmap_andnot_cardinality(
                    lb->bitmaps64[i], lb->bitmaps64[i + 1]);
            }
            return static_cast<int64_t>(marker);
        },
        pairs, 50);

    // ---- N-way unions on ALL dataset bitmaps ---------------------------
    add_bench(
        "TotalUnion",
        "bench.cpp TotalUnion: one N-way roaring_bitmap_or_many() across "
        "every bitmap in the dataset. Measures the in-place N-way union "
        "strategy on real data.",
        [](LoadedBitmaps *lb) -> int64_t {
            roaring_bitmap_t *t = roaring_bitmap_or_many(
                lb->bitmaps.size(),
                const_cast<const roaring_bitmap_t **>(lb->bitmaps.data()));
            int64_t card = roaring_bitmap_get_cardinality(t);
            roaring_bitmap_free(t);
            return card;
        },
        count, 5);

    add_bench(
        "TotalUnionHeap",
        "bench.cpp TotalUnionHeap: one N-way roaring_bitmap_or_many_heap() "
        "across every bitmap in the dataset. Uses a priority-queue "
        "pairwise-tournament strategy instead of in-place N-way.",
        [](LoadedBitmaps *lb) -> int64_t {
            roaring_bitmap_t *t = roaring_bitmap_or_many_heap(
                lb->bitmaps.size(),
                const_cast<const roaring_bitmap_t **>(lb->bitmaps.data()));
            int64_t card = roaring_bitmap_get_cardinality(t);
            roaring_bitmap_free(t);
            return card;
        },
        count, 5);

    // ---- Quartile membership queries (GLOBAL max) ----------------------
    // Note: this differs from real_bitmaps/contains_quartiles, which uses
    // per-bitmap maxima. bench.cpp uses the dataset-wide maximum — many
    // queries therefore fall outside any individual bitmap's range.
    add_bench(
        "RandomAccess",
        "bench.cpp RandomAccess: three roaring_bitmap_contains() probes "
        "per bitmap at global_max/4, global_max/2, and 3*global_max/4. "
        "Uses the dataset-wide maximum (matching bench.cpp), so queries "
        "commonly miss on bitmaps whose own maxima are smaller. Compare "
        "with real_bitmaps/contains_quartiles, which uses per-bitmap "
        "quartiles.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            uint32_t m = lb->global_max;
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                marker += roaring_bitmap_contains(lb->bitmaps[i], m / 4);
                marker += roaring_bitmap_contains(lb->bitmaps[i], m / 2);
                marker += roaring_bitmap_contains(lb->bitmaps[i], 3 * m / 4);
            }
            return static_cast<int64_t>(marker);
        },
        3 * count, 200);

    add_bench(
        "RandomAccess64",
        "bench.cpp RandomAccess64: 64-bit version of RandomAccess — "
        "three roaring64_bitmap_contains() probes per 64-bit bitmap at "
        "global_max/4, /2, 3/4.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            uint32_t m = lb->global_max;
            for (size_t i = 0; i < lb->bitmaps64.size(); ++i) {
                marker += roaring64_bitmap_contains(lb->bitmaps64[i], m / 4);
                marker += roaring64_bitmap_contains(lb->bitmaps64[i], m / 2);
                marker +=
                    roaring64_bitmap_contains(lb->bitmaps64[i], 3 * m / 4);
            }
            return static_cast<int64_t>(marker);
        },
        3 * count, 200);

    add_bench(
        "RandomAccess64Cpp",
        "bench.cpp RandomAccess64Cpp: RandomAccess64 through the C++ "
        "Roaring64Map wrapper — three .contains() probes per bitmap.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            uint32_t m = lb->global_max;
            for (size_t i = 0; i < lb->bitmaps64cpp.size(); ++i) {
                marker += lb->bitmaps64cpp[i]->contains(m / 4);
                marker += lb->bitmaps64cpp[i]->contains(m / 2);
                marker += lb->bitmaps64cpp[i]->contains(3 * m / 4);
            }
            return static_cast<int64_t>(marker);
        },
        3 * count, 200);

    // ---- Full to-array export for every bitmap -------------------------
    add_bench(
        "ToArray",
        "bench.cpp ToArray: calls roaring_bitmap_to_uint32_array() for "
        "every bitmap in the dataset into a shared reusable buffer sized "
        "to max_card. Measures bulk export throughput.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                roaring_bitmap_to_uint32_array(lb->bitmaps[i],
                                               lb->array_buffer);
                marker += lb->array_buffer[0];
            }
            return static_cast<int64_t>(marker);
        },
        count, 5);

    add_bench(
        "ToArray64",
        "bench.cpp ToArray64: roaring64_bitmap_to_uint64_array() for "
        "every 64-bit bitmap into a shared reusable uint64_t buffer.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i < lb->bitmaps64.size(); ++i) {
                roaring64_bitmap_to_uint64_array(lb->bitmaps64[i],
                                                 lb->array_buffer64);
                marker += lb->array_buffer64[0];
            }
            return static_cast<int64_t>(marker);
        },
        count, 5);

    // ---- Iterate-all over every bitmap ---------------------------------
    add_bench(
        "IterateAll",
        "bench.cpp IterateAll: walks every value of every 32-bit bitmap "
        "in the dataset using the C iterator "
        "(roaring_uint32_iterator_init / _advance). Counts total "
        "iterations rather than summing values — matches bench.cpp.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                roaring_uint32_iterator_t j;
                roaring_iterator_init(lb->bitmaps[i], &j);
                while (j.has_value) {
                    marker++;
                    roaring_uint32_iterator_advance(&j);
                }
            }
            return static_cast<int64_t>(marker);
        },
        count, 5);

    add_bench(
        "IterateAll64",
        "bench.cpp IterateAll64: walks every value of every 64-bit bitmap "
        "using roaring64_iterator_create / _advance. Counts total "
        "iterations.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i < lb->bitmaps64.size(); ++i) {
                roaring64_iterator_t *it =
                    roaring64_iterator_create(lb->bitmaps64[i]);
                while (roaring64_iterator_has_value(it)) {
                    marker++;
                    roaring64_iterator_advance(it);
                }
                roaring64_iterator_free(it);
            }
            return static_cast<int64_t>(marker);
        },
        count, 3);

    // ---- Cardinality loops --------------------------------------------
    add_bench(
        "ComputeCardinality",
        "bench.cpp ComputeCardinality: sums roaring_bitmap_get_cardinality() "
        "across every bitmap. With run-optimised containers this is a "
        "sum of cached counts — essentially free per bitmap.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                marker += roaring_bitmap_get_cardinality(lb->bitmaps[i]);
            }
            return static_cast<int64_t>(marker);
        },
        count, 1000);

    add_bench(
        "ComputeCardinality64",
        "bench.cpp ComputeCardinality64: sums "
        "roaring64_bitmap_get_cardinality() across every 64-bit bitmap.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            for (size_t i = 0; i < lb->bitmaps64.size(); ++i) {
                marker += roaring64_bitmap_get_cardinality(lb->bitmaps64[i]);
            }
            return static_cast<int64_t>(marker);
        },
        count, 1000);

    // ---- Rank -----------------------------------------------------------
    add_bench(
        "RankManySlow",
        "bench.cpp RankManySlow: five individual roaring_bitmap_rank() "
        "calls (at global_max/5, 2/5, 3/5, 4/5, and global_max) per "
        "bitmap. Baseline against the vectorised RankMany variant.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t r = 0;
            uint32_t m = lb->global_max;
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                r += roaring_bitmap_rank(lb->bitmaps[i], m / 5);
                r += roaring_bitmap_rank(lb->bitmaps[i], 2 * m / 5);
                r += roaring_bitmap_rank(lb->bitmaps[i], 3 * m / 5);
                r += roaring_bitmap_rank(lb->bitmaps[i], 4 * m / 5);
                r += roaring_bitmap_rank(lb->bitmaps[i], m);
            }
            return static_cast<int64_t>(r);
        },
        5 * count, 50);

    add_bench(
        "RankMany",
        "bench.cpp RankMany: one batched roaring_bitmap_rank_many() call "
        "per bitmap, supplying the same five quintile targets. Compare "
        "against RankManySlow to see the batched speedup.",
        [](LoadedBitmaps *lb) -> int64_t {
            uint64_t marker = 0;
            uint32_t m = lb->global_max;
            std::vector<uint32_t> input{m / 5, 2 * m / 5, 3 * m / 5, 4 * m / 5,
                                        m};
            std::vector<uint64_t> ranks(5);
            for (size_t i = 0; i < lb->bitmaps.size(); ++i) {
                roaring_bitmap_rank_many(lb->bitmaps[i], input.data(),
                                         input.data() + input.size(),
                                         ranks.data());
                marker += ranks[0];
            }
            return static_cast<int64_t>(marker);
        },
        5 * count, 50);
}

}  // namespace microbench

// --------------------------------------------- Roaring64Map fastunion

namespace fastunion64 {

constexpr uint32_t num_bitmaps = 100;
constexpr uint32_t num_outer_slots = 1000;
constexpr uint32_t num_inner_values = 2000;

struct S {
    std::vector<Roaring64Map> maps;
    std::vector<const Roaring64Map *> ptrs;
};

S *build() {
    auto *s = new S;
    s->maps.reserve(num_bitmaps);
    for (uint32_t bm = 0; bm < num_bitmaps; ++bm) {
        Roaring64Map r;
        for (uint32_t slot = 0; slot < num_outer_slots; ++slot) {
            uint64_t value =
                (static_cast<uint64_t>(slot) << 32) + bm + 0x98765432ULL;
            for (uint32_t i = 0; i < num_inner_values; ++i) {
                r.add(value);
                value += num_bitmaps;
            }
        }
        s->maps.push_back(std::move(r));
    }
    for (const auto &m : s->maps) s->ptrs.push_back(&m);
    return s;
}

void register_benchmarks(std::vector<Entry> &out) {
    {
        Entry e;
        e.name = "fastunion64/legacy";
        e.description =
            "Unions 100 synthetically-generated Roaring64Map bitmaps, "
            "each containing 2,000,000 values spread across 1,000 "
            "outer 32-bit slots with a stride that forces dense packing "
            "in the result. Uses the legacy reduce pattern: start with "
            "an empty Roaring64Map, then fold each input in with |=. "
            "This is the baseline the dedicated fastunion must beat. "
            "Building the 100 inputs is expensive, so the inputs are "
            "shared across iterations.";
        e.setup = []() -> void * { return build(); };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            Roaring64Map ans;
            for (size_t i = 0; i < s->ptrs.size(); ++i) ans |= *s->ptrs[i];
            return static_cast<int64_t>(ans.cardinality());
        };
        e.teardown = [](void *sv) { delete static_cast<S *>(sv); };
        e.ops_per_run = num_bitmaps;
        e.reusable_state = true;
        out.push_back(std::move(e));
    }
    {
        Entry e;
        e.name = "fastunion64/fastunion";
        e.description =
            "Unions the same 100 Roaring64Map inputs as fastunion64/legacy "
            "but via Roaring64Map::fastunion(n, ptrs) — a single call "
            "that internally dispatches to the 32-bit or_many machinery "
            "per outer slot. Expected to beat the legacy fold by a wide "
            "margin because it avoids reconstructing the high-word tree "
            "on every OR. Inputs shared across iterations.";
        e.setup = []() -> void * { return build(); };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            Roaring64Map ans =
                Roaring64Map::fastunion(s->ptrs.size(), s->ptrs.data());
            return static_cast<int64_t>(ans.cardinality());
        };
        e.teardown = [](void *sv) { delete static_cast<S *>(sv); };
        e.ops_per_run = num_bitmaps;
        e.reusable_state = true;
        out.push_back(std::move(e));
    }
}
}  // namespace fastunion64

// --------------------------------------------- sparse cases (Roaring64Map)

namespace sparse64 {

void register_benchmarks(std::vector<Entry> &out) {
    struct Cfg {
        size_t batch;
        uint64_t max;
        const char *tag;
    };
    // Shortened from the original set — keep representative combinations.
    const Cfg cfgs[] = {
        {100, 100, "b=100/max=1e2"},
        {100, 1000, "b=100/max=1e3"},
        {100, 1000000, "b=100/max=1e6"},
        {100, 100000000, "b=100/max=1e8"},
        {100000, 1000, "b=1e5/max=1e3"},
        {100000, 100000, "b=1e5/max=1e5"},
        {100000, 1000000000ull, "b=1e5/max=1e9"},
    };
    for (const Cfg &c : cfgs) {
        struct S {
            size_t batch;
            uint64_t max;
        };
        Entry e;
        e.name = std::string("sparse64/insert/") + c.tag;
        size_t batch = c.batch;
        uint64_t maxv = c.max;
        char dbuf[768];
        snprintf(dbuf, sizeof(dbuf),
                 "Inserts %zu uniformly random 64-bit values drawn from "
                 "[0, %" PRIu64
                 "] into a fresh Roaring64Map. The "
                 "(batch, max) pair controls density and locality: with "
                 "batch %zu vs max %" PRIu64
                 " the expected collision "
                 "rate and number of distinct 32-bit high-words per "
                 "insert differ by orders of magnitude across the "
                 "sparse64 configurations, which exposes the cost of "
                 "container-creation, hash-map lookups in the 64-bit "
                 "map, and container promotions. A deterministic seed "
                 "keeps the sequence reproducible across iterations.",
                 batch, maxv, batch, maxv);
        e.description = dbuf;
        e.setup = [batch, maxv]() -> void * {
            auto *s = new S;
            s->batch = batch;
            s->max = maxv;
            return s;
        };
        e.run = [](void *sv) -> int64_t {
            auto *s = static_cast<S *>(sv);
            std::default_random_engine rng(12345u);
            std::uniform_int_distribution<uint64_t> dist(0, s->max);
            Roaring64Map bm;
            for (size_t i = 0; i < s->batch; ++i) bm.add(dist(rng));
            return static_cast<int64_t>(bm.cardinality());
        };
        e.teardown = [](void *sv) { delete static_cast<S *>(sv); };
        e.ops_per_run = static_cast<int64_t>(c.batch);
        // Small-batch configurations (b=100) finish in well under a
        // microsecond; bump inner_reps so the timed window comfortably
        // exceeds Apple's 41.67 ns CLOCK_MONOTONIC tick and the kpc-read
        // overhead. Large-batch configurations (b=1e5) are already long
        // enough at inner_reps=1.
        e.inner_reps = (c.batch < 1000) ? 500 : 1;
        out.push_back(std::move(e));
    }
}
}  // namespace sparse64

// --------------------------------------------- synthetic_bench.cpp ports
//
// One-to-one ports of every benchmark in microbenchmarks/synthetic_bench.cpp.
// Each parametric benchmark is expanded over the same
// kCountAndDensityRange matrix google-benchmark uses (4 counts × 7 steps =
// 28 combinations) and the same 10-bitmask random pattern. Three variants
// per operation: roaring64 C API (r64), Roaring64Map C++ wrapper (cpp),
// std::set baseline. Names embed the parameters:
//
//   synthetic/r64ContainsHit/count=1000/step=1
//   synthetic/cppPortableSerialize/count=1000000/step=16777216
//   synthetic/setContainsRandom/bitmask=5
//
namespace synthetic {

// google-benchmark's CreateRange(1000, 1000000, 10) yields these four.
static constexpr size_t kCounts[] = {1000, 10000, 100000, 1000000};
// google-benchmark's CreateRange(1, 1<<48, 256) yields these seven.
static const uint64_t kSteps[] = {
    1ULL,
    256ULL,
    65536ULL,
    16777216ULL,
    4294967296ULL,       // 2^32
    1099511627776ULL,    // 2^40
    281474976710656ULL,  // 2^48
};
// The 10 bitmasks from synthetic_bench.cpp used for "random" variants.
static constexpr uint64_t kBitmasks[] = {
    0x00000000000FFFFFULL, 0x0000000FFFFF0000ULL, 0x000FFFFF00000000ULL,
    0xFFFFF00000000000ULL, 0x000000005DBFC83EULL, 0x00005DBFC83E0000ULL,
    0x5DBFC83E00000000ULL, 0x0000493B189604B6ULL, 0x493B189604B60000ULL,
    0x420C684950A2D088ULL};

// Thread-local RNG for the random-bitmask variants. Seeded deterministically
// so runs are reproducible (unlike the std::random_device + mt19937 in
// synthetic_bench.cpp which is intentionally random).
static inline uint64_t rand_u64(std::mt19937_64 &rng) {
    return std::uniform_int_distribution<uint64_t>(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max())(rng);
}

// ---- helper: print "count=1000000/step=16777216" into a filter-friendly
// name fragment --------------------------------------------------------
static std::string fmt_params(size_t count, uint64_t step) {
    char buf[64];
    snprintf(buf, sizeof(buf), "count=%zu/step=%" PRIu64, count, step);
    return buf;
}

// ====== ContainsHit / ContainsMiss (r64, cpp, set) ===================

struct r64HitState {
    roaring64_bitmap_t *r;
    size_t count;
    uint64_t step;
    size_t i;
};
struct cppHitState {
    Roaring64Map *r;
    size_t count;
    uint64_t step;
    size_t i;
};
struct setHitState {
    std::set<uint64_t> *s;
    size_t count;
    uint64_t step;
    size_t i;
};

static void register_contains_variants(std::vector<Entry> &out) {
    for (size_t count : kCounts) {
        for (uint64_t step : kSteps) {
            std::string ptag = fmt_params(count, step);

            // r64ContainsHit
            {
                Entry e;
                e.name = "synthetic/r64ContainsHit/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64ContainsHit: preload a "
                    "roaring64_bitmap_t with `count` values at positions "
                    "{0, step, 2*step, ..., (count-1)*step}; each "
                    "measured call does one roaring64_bitmap_contains() "
                    "for a value guaranteed to be present, cycling "
                    "through the stored values.";
                e.setup = [count, step]() -> void * {
                    auto *s = new r64HitState{roaring64_bitmap_create(), count,
                                              step, 0};
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<r64HitState *>(sv);
                    uint64_t v = s->i * s->step;
                    s->i = (s->i + 1) % s->count;
                    return roaring64_bitmap_contains(s->r, v) ? 1 : 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<r64HitState *>(sv);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = 1;
                e.inner_reps = 10000;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // cppContainsHit
            {
                Entry e;
                e.name = "synthetic/cppContainsHit/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppContainsHit: same as "
                    "r64ContainsHit but through the Roaring64Map C++ "
                    "wrapper (one .contains(v) per measured call).";
                e.setup = [count, step]() -> void * {
                    auto *s =
                        new cppHitState{new Roaring64Map(), count, step, 0};
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<cppHitState *>(sv);
                    uint64_t v = s->i * s->step;
                    s->i = (s->i + 1) % s->count;
                    return s->r->contains(v) ? 1 : 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<cppHitState *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = 1;
                e.inner_reps = 10000;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // setContainsHit
            {
                Entry e;
                e.name = "synthetic/setContainsHit/" + ptag;
                e.description =
                    "synthetic_bench.cpp setContainsHit: std::set<uint64_t> "
                    "baseline for the same count×step cycling-hit query "
                    "pattern. Each call is `set.find(v) != set.end()`.";
                e.setup = [count, step]() -> void * {
                    auto *s = new setHitState{new std::set<uint64_t>(), count,
                                              step, 0};
                    for (size_t i = 0; i < count; ++i) s->s->insert(i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<setHitState *>(sv);
                    uint64_t v = s->i * s->step;
                    s->i = (s->i + 1) % s->count;
                    return s->s->find(v) != s->s->end() ? 1 : 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<setHitState *>(sv);
                    delete s->s;
                    delete s;
                };
                e.ops_per_run = 1;
                e.inner_reps = 10000;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }

            // r64ContainsMiss
            {
                Entry e;
                e.name = "synthetic/r64ContainsMiss/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64ContainsMiss: same preload as "
                    "r64ContainsHit, but each query is at (i+1)*step-1 — "
                    "a value that is not in the bitmap. Measures the "
                    "negative-lookup path.";
                e.setup = [count, step]() -> void * {
                    auto *s = new r64HitState{roaring64_bitmap_create(), count,
                                              step, 0};
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<r64HitState *>(sv);
                    uint64_t v = (s->i + 1) * s->step - 1;
                    s->i = (s->i + 1) % s->count;
                    return roaring64_bitmap_contains(s->r, v) ? 1 : 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<r64HitState *>(sv);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = 1;
                e.inner_reps = 10000;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            {
                Entry e;
                e.name = "synthetic/cppContainsMiss/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppContainsMiss: miss pattern "
                    "via Roaring64Map::contains().";
                e.setup = [count, step]() -> void * {
                    auto *s =
                        new cppHitState{new Roaring64Map(), count, step, 0};
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<cppHitState *>(sv);
                    uint64_t v = (s->i + 1) * s->step - 1;
                    s->i = (s->i + 1) % s->count;
                    return s->r->contains(v) ? 1 : 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<cppHitState *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = 1;
                e.inner_reps = 10000;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            {
                Entry e;
                e.name = "synthetic/setContainsMiss/" + ptag;
                e.description =
                    "synthetic_bench.cpp setContainsMiss: miss pattern "
                    "against std::set.";
                e.setup = [count, step]() -> void * {
                    auto *s = new setHitState{new std::set<uint64_t>(), count,
                                              step, 0};
                    for (size_t i = 0; i < count; ++i) s->s->insert(i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<setHitState *>(sv);
                    uint64_t v = (s->i + 1) * s->step - 1;
                    s->i = (s->i + 1) % s->count;
                    return s->s->find(v) != s->s->end() ? 1 : 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<setHitState *>(sv);
                    delete s->s;
                    delete s;
                };
                e.ops_per_run = 1;
                e.inner_reps = 10000;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
        }
    }
}

// ====== ContainsRandom / InsertRemoveRandom (per bitmask) ============

struct r64RandState {
    roaring64_bitmap_t *r;
    uint64_t bitmask;
    std::mt19937_64 rng;
};
struct cppRandState {
    Roaring64Map *r;
    uint64_t bitmask;
    std::mt19937_64 rng;
};
struct setRandState {
    std::set<uint64_t> *s;
    uint64_t bitmask;
    std::mt19937_64 rng;
};

static void register_random_variants(std::vector<Entry> &out) {
    for (size_t bi = 0; bi < sizeof(kBitmasks) / sizeof(kBitmasks[0]); ++bi) {
        uint64_t mask = kBitmasks[bi];
        char tag[32];
        snprintf(tag, sizeof(tag), "bitmask=%zu", bi);

        // r64ContainsRandom
        {
            Entry e;
            e.name = std::string("synthetic/r64ContainsRandom/") + tag;
            e.description =
                "synthetic_bench.cpp r64ContainsRandom: preload a "
                "roaring64_bitmap_t with 2^20 values randomly chosen via "
                "rand() & bitmask, then time one contains() per call on "
                "fresh random (& bitmask) values. The 10 bitmasks cover "
                "20, 32, 48, and 64 bit spreads.";
            e.setup = [mask]() -> void * {
                auto *s =
                    new r64RandState{roaring64_bitmap_create(), mask,
                                     std::mt19937_64(0xdeadbeefULL + mask)};
                for (size_t i = 0; i < (1U << 20); ++i)
                    roaring64_bitmap_add(s->r, rand_u64(s->rng) & mask);
                return s;
            };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<r64RandState *>(sv);
                uint64_t v = rand_u64(s->rng) & s->bitmask;
                return roaring64_bitmap_contains(s->r, v) ? 1 : 0;
            };
            e.teardown = [](void *sv) {
                auto *s = static_cast<r64RandState *>(sv);
                roaring64_bitmap_free(s->r);
                delete s;
            };
            e.ops_per_run = 1;
            e.inner_reps = 10000;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        // cppContainsRandom
        {
            Entry e;
            e.name = std::string("synthetic/cppContainsRandom/") + tag;
            e.description =
                "synthetic_bench.cpp cppContainsRandom: random-bitmask "
                "contains through the Roaring64Map C++ wrapper.";
            e.setup = [mask]() -> void * {
                auto *s =
                    new cppRandState{new Roaring64Map(), mask,
                                     std::mt19937_64(0xdeadbeefULL + mask)};
                for (size_t i = 0; i < (1U << 20); ++i)
                    s->r->add(rand_u64(s->rng) & mask);
                return s;
            };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<cppRandState *>(sv);
                uint64_t v = rand_u64(s->rng) & s->bitmask;
                return s->r->contains(v) ? 1 : 0;
            };
            e.teardown = [](void *sv) {
                auto *s = static_cast<cppRandState *>(sv);
                delete s->r;
                delete s;
            };
            e.ops_per_run = 1;
            e.inner_reps = 10000;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        // setContainsRandom
        {
            Entry e;
            e.name = std::string("synthetic/setContainsRandom/") + tag;
            e.description =
                "synthetic_bench.cpp setContainsRandom: random-bitmask "
                "contains against std::set<uint64_t>.";
            e.setup = [mask]() -> void * {
                auto *s =
                    new setRandState{new std::set<uint64_t>(), mask,
                                     std::mt19937_64(0xdeadbeefULL + mask)};
                for (size_t i = 0; i < (1U << 20); ++i)
                    s->s->insert(rand_u64(s->rng) & mask);
                return s;
            };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<setRandState *>(sv);
                uint64_t v = rand_u64(s->rng) & s->bitmask;
                return s->s->find(v) != s->s->end() ? 1 : 0;
            };
            e.teardown = [](void *sv) {
                auto *s = static_cast<setRandState *>(sv);
                delete s->s;
                delete s;
            };
            e.ops_per_run = 1;
            e.inner_reps = 10000;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }

        // r64InsertRemoveRandom
        {
            Entry e;
            e.name = std::string("synthetic/r64InsertRemoveRandom/") + tag;
            e.description =
                "synthetic_bench.cpp r64InsertRemoveRandom: preload a "
                "roaring64_bitmap_t with 2^20 random (& bitmask) values, "
                "then each call adds one fresh random value and removes "
                "another. Reports per-pair cost (insert + remove).";
            e.setup = [mask]() -> void * {
                auto *s =
                    new r64RandState{roaring64_bitmap_create(), mask,
                                     std::mt19937_64(0xdeadbeefULL + mask)};
                for (size_t i = 0; i < (1U << 20); ++i)
                    roaring64_bitmap_add(s->r, rand_u64(s->rng) & mask);
                return s;
            };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<r64RandState *>(sv);
                uint64_t a = rand_u64(s->rng) & s->bitmask;
                uint64_t r = rand_u64(s->rng) & s->bitmask;
                roaring64_bitmap_add(s->r, a);
                roaring64_bitmap_remove(s->r, r);
                return 0;
            };
            e.teardown = [](void *sv) {
                auto *s = static_cast<r64RandState *>(sv);
                roaring64_bitmap_free(s->r);
                delete s;
            };
            e.ops_per_run = 2;
            e.inner_reps = 5000;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = std::string("synthetic/cppInsertRemoveRandom/") + tag;
            e.description =
                "synthetic_bench.cpp cppInsertRemoveRandom: paired "
                "add/remove through Roaring64Map.";
            e.setup = [mask]() -> void * {
                auto *s =
                    new cppRandState{new Roaring64Map(), mask,
                                     std::mt19937_64(0xdeadbeefULL + mask)};
                for (size_t i = 0; i < (1U << 20); ++i)
                    s->r->add(rand_u64(s->rng) & mask);
                return s;
            };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<cppRandState *>(sv);
                uint64_t a = rand_u64(s->rng) & s->bitmask;
                uint64_t r = rand_u64(s->rng) & s->bitmask;
                s->r->add(a);
                s->r->remove(r);
                return 0;
            };
            e.teardown = [](void *sv) {
                auto *s = static_cast<cppRandState *>(sv);
                delete s->r;
                delete s;
            };
            e.ops_per_run = 2;
            e.inner_reps = 5000;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
        {
            Entry e;
            e.name = std::string("synthetic/setInsertRemoveRandom/") + tag;
            e.description =
                "synthetic_bench.cpp setInsertRemoveRandom: paired "
                "insert/erase against std::set.";
            e.setup = [mask]() -> void * {
                auto *s =
                    new setRandState{new std::set<uint64_t>(), mask,
                                     std::mt19937_64(0xdeadbeefULL + mask)};
                for (size_t i = 0; i < (1U << 20); ++i)
                    s->s->insert(rand_u64(s->rng) & mask);
                return s;
            };
            e.run = [](void *sv) -> int64_t {
                auto *s = static_cast<setRandState *>(sv);
                uint64_t a = rand_u64(s->rng) & s->bitmask;
                uint64_t r = rand_u64(s->rng) & s->bitmask;
                s->s->insert(a);
                s->s->erase(r);
                return 0;
            };
            e.teardown = [](void *sv) {
                auto *s = static_cast<setRandState *>(sv);
                delete s->s;
                delete s;
            };
            e.ops_per_run = 2;
            e.inner_reps = 5000;
            e.reusable_state = true;
            out.push_back(std::move(e));
        }
    }
}

// ====== Insert / Remove (build-from-scratch timed) ===================

struct insertParams {
    size_t count;
    uint64_t step;
};

static void register_insert_remove(std::vector<Entry> &out) {
    for (size_t count : kCounts) {
        for (uint64_t step : kSteps) {
            std::string ptag = fmt_params(count, step);

            // r64Insert — each timed iteration builds a fresh bitmap.
            {
                Entry e;
                e.name = "synthetic/r64Insert/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64Insert: each timed call "
                    "allocates a fresh roaring64_bitmap_t, inserts "
                    "`count` values at positions {0, step, 2*step, ...}, "
                    "and frees it. Measures build-from-scratch cost, "
                    "including allocation/free.";
                e.setup = [count, step]() -> void * {
                    return new insertParams{count, step};
                };
                e.run = [](void *sv) -> int64_t {
                    auto *p = static_cast<insertParams *>(sv);
                    roaring64_bitmap_t *r = roaring64_bitmap_create();
                    for (size_t i = 0; i < p->count; ++i)
                        roaring64_bitmap_add(r, i * p->step);
                    int64_t c = static_cast<int64_t>(
                        roaring64_bitmap_get_cardinality(r));
                    roaring64_bitmap_free(r);
                    return c;
                };
                e.teardown = [](void *sv) {
                    delete static_cast<insertParams *>(sv);
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            {
                Entry e;
                e.name = "synthetic/cppInsert/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppInsert: build-from-scratch "
                    "cost through the Roaring64Map C++ wrapper.";
                e.setup = [count, step]() -> void * {
                    return new insertParams{count, step};
                };
                e.run = [](void *sv) -> int64_t {
                    auto *p = static_cast<insertParams *>(sv);
                    Roaring64Map r;
                    for (size_t i = 0; i < p->count; ++i) r.add(i * p->step);
                    return static_cast<int64_t>(r.cardinality());
                };
                e.teardown = [](void *sv) {
                    delete static_cast<insertParams *>(sv);
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            {
                Entry e;
                e.name = "synthetic/setInsert/" + ptag;
                e.description =
                    "synthetic_bench.cpp setInsert: build-from-scratch "
                    "cost for std::set<uint64_t>.";
                e.setup = [count, step]() -> void * {
                    return new insertParams{count, step};
                };
                e.run = [](void *sv) -> int64_t {
                    auto *p = static_cast<insertParams *>(sv);
                    std::set<uint64_t> s;
                    for (size_t i = 0; i < p->count; ++i) s.insert(i * p->step);
                    return static_cast<int64_t>(s.size());
                };
                e.teardown = [](void *sv) {
                    delete static_cast<insertParams *>(sv);
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.reusable_state = true;
                out.push_back(std::move(e));
            }

            // Remove — setup rebuilds the populated container each iter so
            // the timed run() removes actual data (emulates bench.cpp's
            // state.PauseTiming()/ResumeTiming()).
            {
                Entry e;
                e.name = "synthetic/r64Remove/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64Remove: each measured call "
                    "removes `count` previously-inserted values. Setup "
                    "rebuilds the populated bitmap before every iteration "
                    "so the timed region only covers the removes, "
                    "matching bench.cpp's PauseTiming/ResumeTiming "
                    "pattern.";
                struct S {
                    size_t count;
                    uint64_t step;
                    roaring64_bitmap_t *r;
                };
                e.setup = [count, step]() -> void * {
                    auto *s = new S{count, step, roaring64_bitmap_create()};
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<S *>(sv);
                    for (size_t i = 0; i < s->count; ++i)
                        roaring64_bitmap_remove(s->r, i * s->step);
                    return static_cast<int64_t>(
                        roaring64_bitmap_get_cardinality(s->r));
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<S *>(sv);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                // Not reusable: setup rebuilds the populated state so each
                // measured iteration starts with a full container.
                out.push_back(std::move(e));
            }
            {
                Entry e;
                e.name = "synthetic/cppRemove/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppRemove: remove-all through "
                    "Roaring64Map, with setup rebuilding the populated "
                    "map before each measured iteration.";
                struct S {
                    size_t count;
                    uint64_t step;
                    Roaring64Map *r;
                };
                e.setup = [count, step]() -> void * {
                    auto *s = new S{count, step, new Roaring64Map()};
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<S *>(sv);
                    for (size_t i = 0; i < s->count; ++i)
                        s->r->remove(i * s->step);
                    return static_cast<int64_t>(s->r->cardinality());
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<S *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                out.push_back(std::move(e));
            }
            {
                Entry e;
                e.name = "synthetic/setRemove/" + ptag;
                e.description =
                    "synthetic_bench.cpp setRemove: remove-all through "
                    "std::set<uint64_t>::erase, setup rebuilding before "
                    "each iteration.";
                struct S {
                    size_t count;
                    uint64_t step;
                    std::set<uint64_t> *s;
                };
                e.setup = [count, step]() -> void * {
                    auto *s = new S{count, step, new std::set<uint64_t>()};
                    for (size_t i = 0; i < count; ++i) s->s->insert(i * step);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<S *>(sv);
                    for (size_t i = 0; i < s->count; ++i)
                        s->s->erase(i * s->step);
                    return static_cast<int64_t>(s->s->size());
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<S *>(sv);
                    delete s->s;
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                out.push_back(std::move(e));
            }
        }
    }
}

// ====== Serialize / Deserialize (r64 + cpp; portable + frozen) =======

struct serState {
    roaring64_bitmap_t *r;
    std::vector<char> buf;
};
struct serStateCpp {
    Roaring64Map *r;
    std::vector<char> buf;
    size_t size;
};
struct serStateFrozen {
    roaring64_bitmap_t *r;
    char *buf;
    size_t size;
};
struct serStateCppFrozen {
    Roaring64Map *r;
    std::vector<char> buf;
    size_t size;
};

static void register_ser_deser(std::vector<Entry> &out) {
    for (size_t count : kCounts) {
        for (uint64_t step : kSteps) {
            std::string ptag = fmt_params(count, step);

            // r64PortableSerialize
            {
                Entry e;
                e.name = "synthetic/r64PortableSerialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64PortableSerialize: preload a "
                    "bitmap of `count` values at stride `step`, then time "
                    "one roaring64_bitmap_portable_serialize() call per "
                    "measured iteration into a preallocated buffer.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serState;
                    s->r = roaring64_bitmap_create();
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    s->buf.resize(
                        roaring64_bitmap_portable_size_in_bytes(s->r));
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serState *>(sv);
                    return static_cast<int64_t>(
                        roaring64_bitmap_portable_serialize(s->r,
                                                            s->buf.data()));
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serState *>(sv);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 5;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // r64FrozenSerialize
            {
                Entry e;
                e.name = "synthetic/r64FrozenSerialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64FrozenSerialize: preload a "
                    "bitmap (run-optimised + shrunk), then time one "
                    "roaring64_bitmap_frozen_serialize() call per "
                    "iteration.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serState;
                    s->r = roaring64_bitmap_create();
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    roaring64_bitmap_shrink_to_fit(s->r);
                    s->buf.resize(roaring64_bitmap_frozen_size_in_bytes(s->r));
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serState *>(sv);
                    roaring64_bitmap_frozen_serialize(s->r, s->buf.data());
                    return 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serState *>(sv);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 5;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // cppPortableSerialize
            {
                Entry e;
                e.name = "synthetic/cppPortableSerialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppPortableSerialize: "
                    "Roaring64Map.write(portable=true) into a preallocated "
                    "buffer.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serStateCpp;
                    s->r = new Roaring64Map();
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    s->size = s->r->getSizeInBytes(/*portable=*/true);
                    s->buf.resize(s->size);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serStateCpp *>(sv);
                    return static_cast<int64_t>(
                        s->r->write(s->buf.data(), /*portable=*/true));
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serStateCpp *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 5;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // cppFrozenSerialize
            {
                Entry e;
                e.name = "synthetic/cppFrozenSerialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppFrozenSerialize: "
                    "Roaring64Map.writeFrozen() into a 32-byte aligned "
                    "(overallocated) buffer.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serStateCppFrozen;
                    s->r = new Roaring64Map();
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    s->size = s->r->getFrozenSizeInBytes();
                    s->buf.resize(s->size * 2 + 32);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serStateCppFrozen *>(sv);
                    s->r->writeFrozen(s->buf.data());
                    return 0;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serStateCppFrozen *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 5;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }

            // r64PortableDeserialize
            {
                Entry e;
                e.name = "synthetic/r64PortableDeserialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64PortableDeserialize: preload "
                    "+ serialize to a buffer once in setup, then time "
                    "roaring64_bitmap_portable_deserialize_safe(buf, size) "
                    "+ free per iteration.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serState;
                    s->r = roaring64_bitmap_create();
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    s->buf.resize(
                        roaring64_bitmap_portable_size_in_bytes(s->r));
                    roaring64_bitmap_portable_serialize(s->r, s->buf.data());
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serState *>(sv);
                    roaring64_bitmap_t *r2 =
                        roaring64_bitmap_portable_deserialize_safe(
                            s->buf.data(), s->buf.size());
                    int64_t ok = r2 ? 1 : 0;
                    if (r2) roaring64_bitmap_free(r2);
                    return ok;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serState *>(sv);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 3;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // r64FrozenDeserialize
            {
                Entry e;
                e.name = "synthetic/r64FrozenDeserialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp r64FrozenDeserialize: setup "
                    "produces a 64-byte aligned frozen buffer; timed "
                    "region calls roaring64_bitmap_frozen_view() + free "
                    "per iteration (view construction + teardown, no "
                    "container copy).";
                e.setup = [count, step]() -> void * {
                    auto *s = new serStateFrozen;
                    s->r = roaring64_bitmap_create();
                    for (size_t i = 0; i < count; ++i)
                        roaring64_bitmap_add(s->r, i * step);
                    roaring64_bitmap_shrink_to_fit(s->r);
                    s->size = roaring64_bitmap_frozen_size_in_bytes(s->r);
                    s->buf = static_cast<char *>(
                        roaring_aligned_malloc(64, s->size));
                    roaring64_bitmap_frozen_serialize(s->r, s->buf);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serStateFrozen *>(sv);
                    roaring64_bitmap_t *r2 =
                        roaring64_bitmap_frozen_view(s->buf, s->size);
                    int64_t ok = r2 ? 1 : 0;
                    if (r2) roaring64_bitmap_free(r2);
                    return ok;
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serStateFrozen *>(sv);
                    roaring_aligned_free(s->buf);
                    roaring64_bitmap_free(s->r);
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 20;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // cppPortableDeserialize
            {
                Entry e;
                e.name = "synthetic/cppPortableDeserialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppPortableDeserialize: "
                    "Roaring64Map::read() on a portable buffer prepared "
                    "once in setup.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serStateCpp;
                    s->r = new Roaring64Map();
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    s->size = s->r->getSizeInBytes(/*portable=*/true);
                    s->buf.resize(s->size);
                    s->r->write(s->buf.data(), /*portable=*/true);
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serStateCpp *>(sv);
                    Roaring64Map r2 =
                        Roaring64Map::read(s->buf.data(), /*portable=*/true);
                    return static_cast<int64_t>(r2.cardinality());
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serStateCpp *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 3;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
            // cppFrozenDeserialize
            {
                Entry e;
                e.name = "synthetic/cppFrozenDeserialize/" + ptag;
                e.description =
                    "synthetic_bench.cpp cppFrozenDeserialize: "
                    "Roaring64Map::frozenView() on a frozen buffer "
                    "prepared once in setup.";
                e.setup = [count, step]() -> void * {
                    auto *s = new serStateCppFrozen;
                    s->r = new Roaring64Map();
                    for (size_t i = 0; i < count; ++i) s->r->add(i * step);
                    s->size = s->r->getFrozenSizeInBytes();
                    s->buf.resize(s->size * 2 + 32);
                    s->r->writeFrozen(s->buf.data());
                    return s;
                };
                e.run = [](void *sv) -> int64_t {
                    auto *s = static_cast<serStateCppFrozen *>(sv);
                    Roaring64Map r2 = Roaring64Map::frozenView(s->buf.data());
                    return static_cast<int64_t>(r2.cardinality());
                };
                e.teardown = [](void *sv) {
                    auto *s = static_cast<serStateCppFrozen *>(sv);
                    delete s->r;
                    delete s;
                };
                e.ops_per_run = static_cast<int64_t>(count);
                e.inner_reps = 20;
                e.reusable_state = true;
                out.push_back(std::move(e));
            }
        }
    }
}

static void register_all(std::vector<Entry> &out) {
    register_contains_variants(out);
    register_random_variants(out);
    register_insert_remove(out);
    register_ser_deser(out);
}

}  // namespace synthetic

// ---------------------------------------------------------------- driver

void usage(const char *prog) {
    printf(
        "usage: %s [options]\n"
        "  --filter <substr>     run only benchmarks whose name contains "
        "substr\n"
        "                        (can be repeated to match any of the given "
        "substrings)\n"
        "  --datadir <path>      override auto-discovery with a single dataset "
        "directory\n"
        "                        (e.g. benchmarks/realdata/census-income). "
        "When omitted,\n"
        "                        every subdirectory of the build-time "
        "realdata/ copy is\n"
        "                        loaded as its own dataset.\n"
        "  --ext <ext>           extension used when scanning datasets "
        "(default: .txt)\n"
        "  --list                list registered benchmark names (one per "
        "line) and exit\n"
        "  --list-description    list each benchmark name followed by a "
        "paragraph\n"
        "                        describing what the benchmark measures, then "
        "exit\n"
        "  --markdown            format results as a Markdown table (default)\n"
        "  --csv                 format results as CSV (no header note, "
        "machine-readable)\n"
        "  --min-iter <n>        minimum measured iterations per benchmark "
        "(default 10)\n"
        "  --min-ms <n>          minimum measured wall time per benchmark in "
        "ms (default 200)\n"
        "  -j, --jobs <n>        run benchmarks n-at-a-time on n threads "
        "(default 1).\n"
        "                        Each batch of n finishes before the next "
        "batch starts\n"
        "                        and rows are printed in input order. Hardware "
        "counters\n"
        "                        (cyc/op, ins/op, GHz, ins/cyc, brm/op, "
        "miss/op) are\n"
        "                        DISABLED under -j > 1 because shared caches "
        "and SMT\n"
        "                        contention make per-iteration counts "
        "meaningless.\n"
        "  --sysinfo             print system information\n"
        "  --help                this message\n"
        "\n"
        "Benchmarks whose names contain real_bitmaps/, iteration/, "
        "contains_multi/, or\n"
        "frozen/ are registered once per discovered dataset (suffixed with the "
        "dataset\n"
        "name, e.g. real_bitmaps/contains_quartiles/census-income).\n",
        prog);
}

}  // namespace

int main(int argc, char **argv) {
    bool has_counters = counters::has_performance_counters();
    std::vector<Entry> benchmarks;
    register_array_container(benchmarks);
    register_bitset_container(benchmarks);
    register_run_container(benchmarks);
    register_equals(benchmarks);
    register_create(benchmarks);
    add_bench::register_add_benchmarks(benchmarks);
    adversarial::register_benchmarks(benchmarks);
    intersect_range::register_benchmarks(benchmarks);
    fastunion64::register_benchmarks(benchmarks);
    sparse64::register_benchmarks(benchmarks);
    synthetic::register_all(benchmarks);

    std::vector<std::string> filters;
    const char *datadir = nullptr;
    const char *ext = ".txt";
    bool list_only = false;
    bool list_description = false;
    int min_iter = 10;
    double min_ms = 200.0;
    OutputFormat fmt = OutputFormat::Markdown;
    int jobs = 1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need_value = [&](const char *flag) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s requires a value\n", flag);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--filter")
            filters.push_back(need_value("--filter"));
        else if (a == "--datadir")
            datadir = need_value("--datadir");
        else if (a == "--ext")
            ext = need_value("--ext");
        else if (a == "--list")
            list_only = true;
        else if (a == "--list-description")
            list_description = true;
        else if (a == "--csv")
            fmt = OutputFormat::Csv;
        else if (a == "--markdown")
            fmt = OutputFormat::Markdown;
        else if (a == "--min-iter")
            min_iter = std::atoi(need_value("--min-iter"));
        else if (a == "--min-ms")
            min_ms = std::atof(need_value("--min-ms"));
        else if (a == "-j" || a == "--jobs") {
            jobs = std::atoi(need_value("-j"));
            if (jobs < 1) jobs = 1;
        } else if (a == "--sysinfo")
            tellmeall();
        else if (a == "--help" || a == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", a.c_str());
            usage(argv[0]);
            return 2;
        }
    }
    // Keep every loaded dataset alive for the lifetime of main.
    std::vector<LoadedBitmaps *> all_loaded;
    auto cleanup_loaded = [&all_loaded]() {
        for (LoadedBitmaps *lb : all_loaded) free_loaded(lb);
    };

    if (datadir) {
        // Explicit override: load only that single directory. The dataset
        // name is the basename of the path (e.g. /path/census-income →
        // "census-income").
        LoadedBitmaps *loaded = load_directory(datadir, ext);
        if (!loaded) {
            fprintf(stderr,
                    "no data files with extension %s found in %s; "
                    "skipping real-bitmap benchmarks\n",
                    ext, datadir);
        } else {
            std::string name = datadir;
            while (!name.empty() && name.back() == '/') name.pop_back();
            size_t slash = name.find_last_of('/');
            if (slash != std::string::npos) name = name.substr(slash + 1);
            if (name.empty()) name = "dataset";
            all_loaded.push_back(loaded);
            register_real_bitmaps(benchmarks, loaded, name);
            microbench::register_all(benchmarks, loaded, name);
        }
    } else {
#ifdef BENCHMARK_DATA_DIR
        // Auto-discover every dataset subdirectory under the compile-time
        // default. The CMake build copies benchmarks/realdata/ into the
        // build tree and sets BENCHMARK_DATA_DIR to the copy.
        const char *default_dir = BENCHMARK_DATA_DIR;
        std::vector<std::string> datasets = discover_datasets(default_dir, ext);
        for (const std::string &name : datasets) {
            std::string path = std::string(default_dir) + "/" + name;
            LoadedBitmaps *loaded = load_directory(path.c_str(), ext);
            if (!loaded) continue;
            all_loaded.push_back(loaded);
            register_real_bitmaps(benchmarks, loaded, name);
            microbench::register_all(benchmarks, loaded, name);
        }
#endif
    }

    auto matches = [&](const std::string &name) {
        if (filters.empty()) return true;
        for (const auto &f : filters) {
            if (name.find(f) != std::string::npos) return true;
        }
        return false;
    };

    if (list_only) {
        for (const auto &e : benchmarks) {
            if (matches(e.name)) printf("%s\n", e.name.c_str());
        }
        cleanup_loaded();
        return 0;
    }

    if (list_description) {
        bool first = true;
        for (const auto &e : benchmarks) {
            if (!matches(e.name)) continue;
            if (!first) printf("\n");
            first = false;
            printf("%s\n", e.name.c_str());
            if (e.description.empty()) {
                printf("    (no description registered)\n");
            } else {
                print_wrapped(e.description);
            }
        }
        cleanup_loaded();
        return 0;
    }

    // Disable hardware performance counters when the user asked for
    // parallel execution. The Apple kpc backend reads process-global
    // registers, and on Linux perf_event fds are per-thread but the
    // shared L3/memory subsystem is contended — either way, parallel
    // benchmarks produce meaningless cycle/instruction/cache-miss numbers.
    // ns/op still reflects (loaded) wall-clock time and is kept.
    if (jobs > 1 && has_counters) {
        if (fmt == OutputFormat::Markdown) {
            printf(
                "> Note: hardware performance counters were disabled because "
                "`-j %d` was requested; cycle/instruction/cache-miss "
                "readings are unreliable when benchmarks share the CPU. "
                "Reporting ns/op only.\n\n",
                jobs);
        }
        has_counters = false;
    }

    // In CSV mode, suppress the informational note so the stream stays
    // machine-readable. Markdown output tucks the note under a blockquote
    // so a renderer still shows it, but it doesn't interfere with tables.
    if (!has_counters) {
        if (fmt == OutputFormat::Markdown && jobs <= 1) {
            printf(
                "> Note: hardware performance counters are unavailable on "
                "this platform or with current permissions; reporting "
                "ns/op only.\n");
            printf(
                "> On Linux try running as root or setting "
                "`setcap cap_perfmon+ep` on the executable. On macOS try "
                "running as root or enabling \"Inherit performance "
                "counters\" in Instruments.app.\n\n");
        }
        // In CSV we deliberately emit nothing — the CSV stream is
        // metadata-free by design.
    }

    std::vector<const Entry *> selected;
    for (const auto &e : benchmarks) {
        if (matches(e.name)) selected.push_back(&e);
    }
    if (selected.empty()) {
        fprintf(stderr, "no benchmarks matched filter(s)\n");
        cleanup_loaded();
        return 1;
    }

    // `has_counters` at this point already reflects the -j>1 override
    // (we disabled it above when jobs>1). Pass the same flag as
    // use_counters into run_entry so parallel threads never hit the
    // Apple kpc / Linux perf fd setup at all.
    const bool use_counters = has_counters;
    MdLayout layout = compute_md_layout(selected);
    print_header(has_counters, fmt, layout);
    if (jobs <= 1) {
        for (const Entry *ep : selected) {
            RunResult r = run_entry(*ep, min_iter, min_ms * 1e6,
                                    /*max_iter=*/1000000, use_counters);
            print_row(*ep, r, has_counters, fmt, layout);
        }
    } else {
        // Parallel: run `jobs` benchmarks at a time. Each batch blocks until
        // every thread in it finishes, then rows are printed in input order.
        // This means per-measurement numbers reflect a loaded machine —
        // threads contend for caches, memory bandwidth, and SMT lanes — so
        // results differ from -j 1 and are NOT directly comparable to it.
        for (size_t i = 0; i < selected.size();
             i += static_cast<size_t>(jobs)) {
            size_t end =
                std::min(selected.size(), i + static_cast<size_t>(jobs));
            size_t batch = end - i;
            std::vector<RunResult> results(batch);
            std::vector<std::thread> threads;
            threads.reserve(batch);
            for (size_t k = 0; k < batch; ++k) {
                const Entry *ep = selected[i + k];
                RunResult *slot = &results[k];
                threads.emplace_back(
                    [ep, slot, min_iter, min_ms, use_counters]() {
                        *slot = run_entry(*ep, min_iter, min_ms * 1e6,
                                          /*max_iter=*/1000000, use_counters);
                    });
            }
            for (auto &t : threads) t.join();
            for (size_t k = 0; k < batch; ++k) {
                print_row(*selected[i + k], results[k], has_counters, fmt,
                          layout);
            }
        }
    }

    cleanup_loaded();
    return 0;
}
