//
// cpp_random_unit.cpp
//
// The `roaring_checked.hh` / `roaring64map_checked.hh variations of the C++
// wrapper for roaring bitmaps keep a C++ `std::set` in sync with changes made
// using the object's methods. Those classes have the same name and are in
// namespace `doublecheck`.
//
// This test generates bitsets with randomized content and runs through the
// various operations with them.
//
// The doublecheck code validates the results of API calls, and checks for
// coherence whenever a `Roaring` class is destructed.  Checking for coherence
// can also be done explicitly with `does_std_set_match_roaring()`.
//
// Note: This could be used as the basis for more rigorous "Fuzz Testing" in
// the future, which would be able to integrate further coverage information
// to make sure code paths were being exercised comprehensively:
//
// https://www.llvm.org/docs/LibFuzzer.html
//

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <iostream>
#include <type_traits>
#include <vector>

#include "roaring_checked.hh"
using doublechecked::Roaring;  // so `Roaring` means `doublecheck::Roaring`
#include "roaring64map_checked.hh"
using doublechecked::Roaring64Map;

#include "test.h"

// The tests can run as long as one wants.  Ideally, the sanitizer options
// for `address` and `undefined behavior` should be enabled (see the CMake
// option ROARING_SANITIZE).
//
const unsigned long NUM_STEPS = 1000;

// A batch of bitsets is kept live and recycled as they are operated on against
// each other.  This is how many are kept around at one time.
//
const int NUM_ROARS = 30;

// If we generated data fully at random in the uint32_t / uint64_t space, then
// sets would be unlikely to intersect very often.  Use a rolling focal point to
// kind of distribute the values near enough to each other to be likely to
// interfere.
uint32_t gravity;
uint64_t gravity64;

Roaring make_random_bitset() {
    Roaring r;
    int num_ops = rand() % 100;
    for (int i = 0; i < num_ops; ++i) {
        switch (rand() % 5) {
          case 0:
            r.add(gravity);
            break;

          case 1: {
            uint32_t start = gravity + (rand() % 50) - 25;
            r.addRange(start, start + rand() % 100);
            break; }

          case 2: {
            uint32_t start = gravity + (rand() % 10) - 5;
            r.removeRange(start, start + rand() % 5);
            break; }

          case 3: {
            uint32_t start = gravity + (rand() % 50) - 25;
            r.flip(start, start + rand() % 50);
            break; }

          case 4: {  // tests remove(), select(), rank()
            uint32_t card = r.cardinality();
            if (card != 0) {
                uint32_t rnk = rand() % card;
                uint32_t element;
                assert_true(r.select(rnk, &element));
                assert_int_equal(rnk + 1, r.rank(element));
                r.remove(rnk);
            }
            break; }

          default:
            assert_true(false);
        }
        gravity += (rand() % 200) - 100;
    }
    assert_true(r.does_std_set_match_roaring());
    return r;
}

Roaring64Map make_random_bitset64() {
    Roaring64Map r;
    int num_ops = rand() % 100;
    for (int i = 0; i < num_ops; ++i) {
        switch (rand() % 5) {
            case 0:
                r.add(gravity64);
                break;

            case 1: {
                uint64_t start = gravity64 + (rand() % 50) - 25;
                r.addRange(start, start + rand() % 100);
                break;
            }

            case 2: {
                uint64_t start = gravity64 + (rand() % 10) - 5;
                r.removeRange(start, start + rand() % 5);
                break;
            }

            case 3: {
                uint64_t start = gravity64 + (rand() % 50) - 25;
                r.flip(start, start + rand() % 50);
                break;
            }

            case 4: {  // tests remove(), select(), rank()
                uint64_t card = r.cardinality();
                if (card != 0) {
                    uint64_t rnk = rand() % card;
                    uint64_t element = 0;
                    assert_true(r.select(rnk, &element));
                    assert_int_equal(rnk + 1, r.rank(element));
                    r.remove(rnk);
                }
                break;
            }

            default:
                assert_true(false);
        }
        gravity64 += (rand() % 200) - 100;
    }
    assert_true(r.does_std_set_match_roaring());
    return r;
}

DEFINE_TEST(sanity_check_doublechecking) {
    Roaring r;
    while (r.isEmpty())
        r = make_random_bitset();

    // Pick a random element out of the guaranteed non-empty bitset
    //
    uint32_t rnk = rand() % r.cardinality();
    uint32_t element;
    assert_true(r.select(rnk, &element));

    // Deliberately get check (the std::set) out of sync to ensure match fails
    //
    r.check.erase(element);
    assert_false(r.does_std_set_match_roaring());

    // Put the std::set back in sync so the destructor doesn't assert
    //
    r.check.insert(element);
    assert_true(r.does_std_set_match_roaring());
}

DEFINE_TEST(sanity_check_doublechecking_64) {
    Roaring64Map r;
    while (r.isEmpty()) r = make_random_bitset64();

    // Pick a random element out of the guaranteed non-empty bitset
    //
    uint64_t rnk = rand() % r.cardinality();
    uint64_t element;
    assert_true(r.select(rnk, &element));

    // Deliberately get check (the std::set) out of sync to ensure match fails
    //
    r.check.erase(element);
    assert_false(r.does_std_set_match_roaring());

    // Put the std::set back in sync so the destructor doesn't assert
    //
    r.check.insert(element);
    assert_true(r.does_std_set_match_roaring());
}

DEFINE_TEST(random_doublecheck_test) {
    //
    // Make a group of bitsets to choose from when performing operations.
    //
    std::vector<Roaring> roars;
    for (int i = 0; i < NUM_ROARS; ++i)
        roars.insert(roars.end(), make_random_bitset());

    for (unsigned long step = 0; step < NUM_STEPS; ++step) {
        //
        // Each step modifies the chosen `out` bitset...possibly just
        // overwriting it completely.
        //
        Roaring &out = roars[rand() % NUM_ROARS];

        // The left and right bitsets may be used as inputs for operations.
        // They can be a reference to the same object as out, or can be
        // references to each other (which is good to test those conditions).
        //
        const Roaring &left = roars[rand() % NUM_ROARS];
        const Roaring &right = roars[rand() % NUM_ROARS];

      #ifdef ROARING_CPP_RANDOM_PRINT_STATUS
        printf(
            "[%lu]: %lu %lu %lu\n",
            step,
            static_cast<unsigned long>(left.cardinality()),
            static_cast<unsigned long>(right.cardinality()),
            static_cast<unsigned long>(out.cardinality())
        );
      #endif

        int op = rand() % 6;

        // The "doublecheck" in the C++ wrapper for the non-inplace operations
        // does a check against the inplace version (vs. rewrite the `std::set`
        // code twice).  Hence the inplace and/andnot/or/xor get tested too.
        //
        switch (op) {
          case 0: {  // AND
            uint64_t card = left.and_cardinality(right);
            assert_int_equal(card, right.and_cardinality(left));

            out = left & right;

            assert_int_equal(card, out.cardinality());
            if (&out != &left)
                assert_true(out.isSubset(left));
            if (&out != &right)
                assert_true(out.isSubset(right));
            break; }

          case 1: {  // ANDNOT
            uint64_t card = left.andnot_cardinality(right);

            out = left - right;

            assert_int_equal(card, out.cardinality());
            if ((&out != &left) && (&out != &right))
                assert_int_equal(
                    card, left.cardinality() - right.and_cardinality(left)
                );
            if (&out != &left)
                assert_true(out.isSubset(left));
            if (&out != &right)
                assert_false(out.intersect(right));
            break; }

          case 2: {  // OR
            uint64_t card = left.or_cardinality(right);
            assert_int_equal(card, right.or_cardinality(left));

            out = left | right;

            assert_int_equal(card, out.cardinality());
            if (&out != &left)
                assert_true(left.isSubset(out));
            if (&out != &right)
                assert_true(right.isSubset(out));
            break; }

          case 3: {  // XOR
            uint64_t card = left.xor_cardinality(right);
            assert_true(card == right.xor_cardinality(left));

            out = left ^ right;

            assert_int_equal(card, out.cardinality());
            if ((&out != &left) && (&out != &right)) {
                assert_false(out.intersect(left & right));
                assert_true(
                    card == left.cardinality() + right.cardinality()
                        - (2 * left.and_cardinality(right))
                );
            }
            break; }

          case 4: {  // FASTUNION
            const Roaring *inputs[3] = { &out, &left, &right };
            out = Roaring::fastunion(3, inputs);  // result checked internally
            break; }

          case 5: {  // FLIP
            uint32_t card = out.cardinality();
            if (card != 0) {  // pick gravity point inside set somewhere
                uint32_t rnk = rand() % card;
                uint32_t element;
                assert_true(out.select(rnk, &element));
                assert_int_equal(rnk + 1, out.rank(element));
                gravity = element;
            }
            uint32_t start = gravity + (rand() % 50) - 25;
            out.flip(start, start + rand() % 50);
            break; }

          default:
            assert_true(false);
        }

        // Periodically apply a post-processing step to the out bitset
        //
        int post = rand() % 15;
        switch (post) {
          case 0:
            out.removeRunCompression();
            break;

          case 1:
            out.runOptimize();
            break;

          case 2:
            out.shrinkToFit();
            break;

          default:
            break;
        }

        // Explicitly ask if the `std::set` matches the roaring bitmap in out
        //
        assert_true(out.does_std_set_match_roaring());

        // Do some arbitrary query operations.  No need to test the results, as
        // the doublecheck code ensures the `std::set` matches internally.
        //
        out.isEmpty();
        out.minimum();
        out.maximum();
        out.contains(rand());
        out.containsRange(rand(), rand());
        for (int i = -50; i < 50; ++i) {
            out.contains(gravity + i);
            out.containsRange(gravity + i, rand() % 25);
        }

        // When doing random intersections, the tendency is that sets will
        // lose all their data points over time.  So empty sets are usually
        // re-seeded with more data, but a few get through to test empty cases.
        //
        if (out.isEmpty() && (rand() % 10 != 0))
            out = make_random_bitset();
    }
}

DEFINE_TEST(random_doublecheck_test_64) {
    //
    // Make a group of bitsets to choose from when performing operations.
    //
    std::vector<Roaring64Map> roars;
    for (int i = 0; i < NUM_ROARS; ++i)
        roars.insert(roars.end(), make_random_bitset64());

    for (unsigned long step = 0; step < NUM_STEPS; ++step) {
        //
        // Each step modifies the chosen `out` bitset...possibly just
        // overwriting it completely.
        //
        Roaring64Map &out = roars[rand() % NUM_ROARS];

        // The left and right bitsets may be used as inputs for operations.
        // They can be a reference to the same object as out, or can be
        // references to each other (which is good to test those conditions).
        //
        const Roaring64Map &left = roars[rand() % NUM_ROARS];
        const Roaring64Map &right = roars[rand() % NUM_ROARS];

#ifdef ROARING_CPP_RANDOM_PRINT_STATUS
        printf("[%lu]: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", step,
               left.cardinality(),
               right.cardinality(),
               out.cardinality());
#endif

        int op = rand() % 6;

        switch (op) {
            case 0: {  // AND
                out = left & right;
                if (&out != &left) assert_true(out.isSubset(left));
                if (&out != &right) assert_true(out.isSubset(right));
                break;
            }

            case 1: {  // ANDNOT
                out = left - right;
                if (&out != &left) assert_true(out.isSubset(left));
                break;
            }

            case 2: {  // OR
                out = left | right;
                if (&out != &left) assert_true(left.isSubset(out));
                if (&out != &right) assert_true(right.isSubset(out));
                break;
            }

            case 3: {  // XOR
                out = left ^ right;
                break;
            }

            case 4: {  // FASTUNION
                const Roaring64Map *inputs[3] = {&out, &left, &right};
                out = Roaring64Map::fastunion(
                    3, inputs);  // result checked internally
                break;
            }

            case 5: {  // FLIP
                uint64_t card = out.cardinality();
                if (card != 0) {  // pick gravity point inside set somewhere
                    uint64_t rnk = rand() % card;
                    uint64_t element = 0;
                    assert_true(out.select(rnk, &element));
                    assert_int_equal(rnk + 1, out.rank(element));
                    gravity64 = element;
                }
                uint64_t start = gravity64 + (rand() % 50) - 25;
                out.flip(start, start + rand() % 50);
                break;
            }

            default:
                assert_true(false);
        }

        // Periodically apply a post-processing step to the out bitset
        //
        int post = rand() % 15;
        switch (post) {
            case 0:
                out.removeRunCompression();
                break;

            case 1:
                out.runOptimize();
                break;

            case 2:
                out.shrinkToFit();
                break;

            default:
                break;
        }

        // Explicitly ask if the `std::set` matches the roaring bitmap in out
        //
        assert_true(out.does_std_set_match_roaring());

        // Do some arbitrary query operations.  No need to test the results, as
        // the doublecheck code ensures the `std::set` matches internally.
        //
        out.isEmpty();
        out.minimum();
        out.maximum();
        for (int i = -50; i < 50; ++i) {
            out.contains(gravity64 + i);
        }

        // When doing random intersections, the tendency is that sets will
        // lose all their data points over time.  So empty sets are usually
        // re-seeded with more data, but a few get through to test empty cases.
        //
        if (out.isEmpty() && (rand() % 10 != 0)) out = make_random_bitset64();
    }
}

int main() {
    uint64_t seed = time(nullptr);
    srand(seed);
    printf("Seed:  %" PRIu64 "\n", seed);

    gravity = rand() % 10000;  // starting focal point

    // Make the 64-bit gravity focus around the edge of a 32-bit value to better
    // test edge cases.
    gravity64 = (static_cast<uint64_t>(rand()) << 32) + rand() % 20000 - 10000;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(sanity_check_doublechecking),
        cmocka_unit_test(sanity_check_doublechecking_64),
        cmocka_unit_test(random_doublecheck_test),
        cmocka_unit_test(random_doublecheck_test_64),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
