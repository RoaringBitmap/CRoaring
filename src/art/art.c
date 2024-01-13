#include <assert.h>
#include <roaring/art/art.h>
#include <roaring/memory.h>
#include <stdio.h>
#include <string.h>

#define ART_NODE4_TYPE 0
#define ART_NODE16_TYPE 1
#define ART_NODE48_TYPE 2
#define ART_NODE256_TYPE 3
#define ART_NUM_TYPES 4

// Node48 placeholder value to indicate no child is present at this key index.
#define ART_NODE48_EMPTY_VAL 48

// We use the least significant bit of node pointers to indicate whether a node
// is a leaf or an inner node. This is never surfaced to the user.
//
// Using pointer tagging to indicate leaves not only saves a bit of memory by
// sparing the typecode, but also allows us to use an intrusive leaf struct.
// Using an intrusive leaf struct leaves leaf allocation up to the user. Upon
// deallocation of the ART, we know not to free the leaves without having to
// dereference the leaf pointers.
//
// All internal operations on leaves should use CAST_LEAF before using the leaf.
// The only places that use SET_LEAF are locations where a field is directly
// assigned to a leaf pointer. After using SET_LEAF, the leaf should be treated
// as a node of unknown type.
#define IS_LEAF(p) (((uintptr_t)(p)&1))
#define SET_LEAF(p) ((art_node_t *)((uintptr_t)(p) | 1))
#define CAST_LEAF(p) ((art_leaf_t *)((void *)((uintptr_t)(p) & ~1)))

#ifdef __cplusplus
extern "C" {
namespace roaring {
namespace internal {
#endif

typedef uint8_t art_typecode_t;

// Aliasing with a "leaf" naming so that its purpose is clearer in the context
// of the trie internals.
typedef art_val_t art_leaf_t;

// Inner node, with prefix.
//
// We use a fixed-length array as a pointer would be larger than the array.
typedef struct art_inner_node_s {
    art_typecode_t typecode;
    uint8_t prefix_size;
    uint8_t prefix[ART_KEY_BYTES - 1];
} art_inner_node_t;

// Inner node types.

// Node4: key[i] corresponds with children[i]. Keys are sorted.
typedef struct art_node4_s {
    art_inner_node_t base;
    uint8_t count;
    uint8_t keys[4];
    art_node_t *children[4];
} art_node4_t;

// Node16: key[i] corresponds with children[i]. Keys are sorted.
typedef struct art_node16_s {
    art_inner_node_t base;
    uint8_t count;
    uint8_t keys[16];
    art_node_t *children[16];
} art_node16_t;

// Node48: key[i] corresponds with children[key[i]] if key[i] !=
// ART_NODE48_EMPTY_VAL. Keys are naturally sorted due to direct indexing.
typedef struct art_node48_s {
    art_inner_node_t base;
    uint8_t count;
    uint8_t keys[256];
    art_node_t *children[48];
} art_node48_t;

// Node256: children[i] is directly indexed by key chunk. A child is present if
// children[i] != NULL.
typedef struct art_node256_s {
    art_inner_node_t base;
    uint16_t count;
    art_node_t *children[256];
} art_node256_t;

// Helper struct to refer to a child within a node at a specific index.
typedef struct art_indexed_child_s {
    art_node_t *child;
    uint8_t index;
} art_indexed_child_t;

static inline bool art_is_leaf(const art_node_t *node) { return IS_LEAF(node); }

static void art_leaf_populate(art_leaf_t *leaf, const art_key_chunk_t key[]) {
    memcpy(&leaf->key, key, ART_KEY_BYTES);
}

static inline uint8_t art_get_type(const art_inner_node_t *node) {
    return node->typecode;
}

static inline void art_init_inner_node(art_inner_node_t *node,
                                       art_typecode_t typecode,
                                       const art_key_chunk_t prefix[],
                                       uint8_t prefix_size) {
    node->typecode = typecode;
    node->prefix_size = prefix_size;
    memcpy(node->prefix, prefix, prefix_size * sizeof(art_key_chunk_t));
}

static void art_free_node(art_node_t *node);

// ===================== Start of node-specific functions ======================

static art_node4_t *art_node4_create(const art_key_chunk_t prefix[],
                                     uint8_t prefix_size);
static art_node16_t *art_node16_create(const art_key_chunk_t prefix[],
                                       uint8_t prefix_size);
static art_node48_t *art_node48_create(const art_key_chunk_t prefix[],
                                       uint8_t prefix_size);
static art_node256_t *art_node256_create(const art_key_chunk_t prefix[],
                                         uint8_t prefix_size);

static art_node_t *art_node4_insert(art_node4_t *node, art_node_t *child,
                                    uint8_t key);
static art_node_t *art_node16_insert(art_node16_t *node, art_node_t *child,
                                     uint8_t key);
static art_node_t *art_node48_insert(art_node48_t *node, art_node_t *child,
                                     uint8_t key);
static art_node_t *art_node256_insert(art_node256_t *node, art_node_t *child,
                                      uint8_t key);

static art_node4_t *art_node4_create(const art_key_chunk_t prefix[],
                                     uint8_t prefix_size) {
    art_node4_t *node = (art_node4_t *)roaring_malloc(sizeof(art_node4_t));
    art_init_inner_node(&node->base, ART_NODE4_TYPE, prefix, prefix_size);
    node->count = 0;
    return node;
}

static void art_free_node4(art_node4_t *node) {
    for (size_t i = 0; i < node->count; ++i) {
        art_free_node(node->children[i]);
    }
    roaring_free(node);
}

static inline art_node_t *art_node4_find_child(const art_node4_t *node,
                                               art_key_chunk_t key) {
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] == key) {
            return node->children[i];
        }
    }
    return NULL;
}

static art_node_t *art_node4_insert(art_node4_t *node, art_node_t *child,
                                    uint8_t key) {
    if (node->count < 4) {
        size_t idx = 0;
        for (; idx < node->count; ++idx) {
            if (node->keys[idx] > key) {
                break;
            }
        }
        size_t after = node->count - idx;
        // Shift other keys to maintain sorted order.
        memmove(node->keys + idx + 1, node->keys + idx,
                after * sizeof(art_key_chunk_t));
        memmove(node->children + idx + 1, node->children + idx,
                after * sizeof(art_node_t *));

        node->children[idx] = child;
        node->keys[idx] = key;
        node->count++;
        return (art_node_t *)node;
    }
    art_node16_t *new_node =
        art_node16_create(node->base.prefix, node->base.prefix_size);
    // Instead of calling insert, this could be specialized to 2x memcpy and
    // setting the count.
    for (size_t i = 0; i < 4; ++i) {
        art_node16_insert(new_node, node->children[i], node->keys[i]);
    }
    roaring_free(node);
    return art_node16_insert(new_node, child, key);
}

static inline art_node_t *art_node4_erase(art_node4_t *node,
                                          art_key_chunk_t key_chunk) {
    int idx = -1;
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] == key_chunk) {
            idx = i;
        }
    }
    if (idx == -1) {
        return (art_node_t *)node;
    }
    if (node->count == 2) {
        // Only one child remains after erasing, so compress the path by
        // removing this node.
        art_node_t *remaining_child = node->children[idx ^ 1];
        art_key_chunk_t remaining_child_key = node->keys[idx ^ 1];
        if (!art_is_leaf(remaining_child)) {
            // Correct the prefix of the child node.
            art_inner_node_t *inner_node = (art_inner_node_t *)remaining_child;
            memmove(inner_node->prefix + node->base.prefix_size + 1,
                    inner_node->prefix, inner_node->prefix_size);
            memcpy(inner_node->prefix, node->base.prefix,
                   node->base.prefix_size);
            inner_node->prefix[node->base.prefix_size] = remaining_child_key;
            inner_node->prefix_size += node->base.prefix_size + 1;
        }
        roaring_free(node);
        return remaining_child;
    }
    // Shift other keys to maintain sorted order.
    size_t after_next = node->count - idx - 1;
    memmove(node->keys + idx, node->keys + idx + 1,
            after_next * sizeof(art_key_chunk_t));
    memmove(node->children + idx, node->children + idx + 1,
            after_next * sizeof(art_node_t *));
    node->count--;
    return (art_node_t *)node;
}

static inline void art_node4_replace(art_node4_t *node,
                                     art_key_chunk_t key_chunk,
                                     art_node_t *new_child) {
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] == key_chunk) {
            node->children[i] = new_child;
            return;
        }
    }
}

static inline art_indexed_child_t art_node4_next_child(const art_node4_t *node,
                                                       int index) {
    art_indexed_child_t indexed_child;
    index++;
    if (index >= node->count) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node4_prev_child(const art_node4_t *node,
                                                       int index) {
    if (index > node->count) {
        index = node->count;
    }
    index--;
    art_indexed_child_t indexed_child;
    if (index < 0) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node4_child_at(const art_node4_t *node,
                                                     int index) {
    art_indexed_child_t indexed_child;
    if (index < 0 || index >= node->count) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node4_lower_bound(
    art_node4_t *node, art_key_chunk_t key_chunk) {
    art_indexed_child_t indexed_child;
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] >= key_chunk) {
            indexed_child.index = i;
            indexed_child.child = node->children[i];
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static art_node16_t *art_node16_create(const art_key_chunk_t prefix[],
                                       uint8_t prefix_size) {
    art_node16_t *node = (art_node16_t *)roaring_malloc(sizeof(art_node16_t));
    art_init_inner_node(&node->base, ART_NODE16_TYPE, prefix, prefix_size);
    node->count = 0;
    return node;
}

static void art_free_node16(art_node16_t *node) {
    for (size_t i = 0; i < node->count; ++i) {
        art_free_node(node->children[i]);
    }
    roaring_free(node);
}

static inline art_node_t *art_node16_find_child(const art_node16_t *node,
                                                art_key_chunk_t key) {
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] == key) {
            return node->children[i];
        }
    }
    return NULL;
}

static art_node_t *art_node16_insert(art_node16_t *node, art_node_t *child,
                                     uint8_t key) {
    if (node->count < 16) {
        size_t idx = 0;
        for (; idx < node->count; ++idx) {
            if (node->keys[idx] > key) {
                break;
            }
        }
        size_t after = node->count - idx;
        // Shift other keys to maintain sorted order.
        memmove(node->keys + idx + 1, node->keys + idx,
                after * sizeof(art_key_chunk_t));
        memmove(node->children + idx + 1, node->children + idx,
                after * sizeof(art_node_t *));

        node->children[idx] = child;
        node->keys[idx] = key;
        node->count++;
        return (art_node_t *)node;
    }
    art_node48_t *new_node =
        art_node48_create(node->base.prefix, node->base.prefix_size);
    for (size_t i = 0; i < 16; ++i) {
        art_node48_insert(new_node, node->children[i], node->keys[i]);
    }
    roaring_free(node);
    return art_node48_insert(new_node, child, key);
}

static inline art_node_t *art_node16_erase(art_node16_t *node,
                                           uint8_t key_chunk) {
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] == key_chunk) {
            // Shift other keys to maintain sorted order.
            size_t after_next = node->count - i - 1;
            memmove(node->keys + i, node->keys + i + 1,
                    after_next * sizeof(key_chunk));
            memmove(node->children + i, node->children + i + 1,
                    after_next * sizeof(art_node_t *));
            node->count--;
            break;
        }
    }
    if (node->count > 4) {
        return (art_node_t *)node;
    }
    art_node4_t *new_node =
        art_node4_create(node->base.prefix, node->base.prefix_size);
    // Instead of calling insert, this could be specialized to 2x memcpy and
    // setting the count.
    for (size_t i = 0; i < 4; ++i) {
        art_node4_insert(new_node, node->children[i], node->keys[i]);
    }
    roaring_free(node);
    return (art_node_t *)new_node;
}

static inline void art_node16_replace(art_node16_t *node,
                                      art_key_chunk_t key_chunk,
                                      art_node_t *new_child) {
    for (uint8_t i = 0; i < node->count; ++i) {
        if (node->keys[i] == key_chunk) {
            node->children[i] = new_child;
            return;
        }
    }
}

static inline art_indexed_child_t art_node16_next_child(
    const art_node16_t *node, int index) {
    art_indexed_child_t indexed_child;
    index++;
    if (index >= node->count) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node16_prev_child(
    const art_node16_t *node, int index) {
    if (index > node->count) {
        index = node->count;
    }
    index--;
    art_indexed_child_t indexed_child;
    if (index < 0) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node16_child_at(const art_node16_t *node,
                                                      int index) {
    art_indexed_child_t indexed_child;
    if (index < 0 || index >= node->count) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node16_lower_bound(
    art_node16_t *node, art_key_chunk_t key_chunk) {
    art_indexed_child_t indexed_child;
    for (size_t i = 0; i < node->count; ++i) {
        if (node->keys[i] >= key_chunk) {
            indexed_child.index = i;
            indexed_child.child = node->children[i];
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static art_node48_t *art_node48_create(const art_key_chunk_t prefix[],
                                       uint8_t prefix_size) {
    art_node48_t *node = (art_node48_t *)roaring_malloc(sizeof(art_node48_t));
    art_init_inner_node(&node->base, ART_NODE48_TYPE, prefix, prefix_size);
    node->count = 0;
    for (size_t i = 0; i < 256; ++i) {
        node->keys[i] = ART_NODE48_EMPTY_VAL;
    }
    return node;
}

static void art_free_node48(art_node48_t *node) {
    for (size_t i = 0; i < 256; ++i) {
        uint8_t val_idx = node->keys[i];
        if (val_idx != ART_NODE48_EMPTY_VAL) {
            art_free_node(node->children[val_idx]);
        }
    }
    roaring_free(node);
}

static inline art_node_t *art_node48_find_child(const art_node48_t *node,
                                                art_key_chunk_t key) {
    uint8_t val_idx = node->keys[key];
    if (val_idx != ART_NODE48_EMPTY_VAL) {
        return node->children[val_idx];
    }
    return NULL;
}

static art_node_t *art_node48_insert(art_node48_t *node, art_node_t *child,
                                     uint8_t key) {
    if (node->count < 48) {
        uint8_t val_idx = node->count;
        node->keys[key] = val_idx;
        node->children[val_idx] = child;
        node->count++;
        return (art_node_t *)node;
    }
    art_node256_t *new_node =
        art_node256_create(node->base.prefix, node->base.prefix_size);
    for (size_t i = 0; i < 256; ++i) {
        uint8_t val_idx = node->keys[i];
        if (val_idx != ART_NODE48_EMPTY_VAL) {
            art_node256_insert(new_node, node->children[val_idx], i);
        }
    }
    roaring_free(node);
    return art_node256_insert(new_node, child, key);
}

static inline art_node_t *art_node48_erase(art_node48_t *node,
                                           uint8_t key_chunk) {
    uint8_t val_idx = node->keys[key_chunk];
    if (val_idx == ART_NODE48_EMPTY_VAL) {
        return (art_node_t *)node;
    }
    node->children[val_idx] = NULL;
    node->keys[key_chunk] = ART_NODE48_EMPTY_VAL;
    node->count--;
    if (node->count > 16) {
        return (art_node_t *)node;
    }

    art_node16_t *new_node =
        art_node16_create(node->base.prefix, node->base.prefix_size);
    for (size_t i = 0; i < 256; ++i) {
        val_idx = node->keys[i];
        if (val_idx != ART_NODE48_EMPTY_VAL) {
            art_node16_insert(new_node, node->children[val_idx], i);
        }
    }
    roaring_free(node);
    return (art_node_t *)new_node;
}

static inline void art_node48_replace(art_node48_t *node,
                                      art_key_chunk_t key_chunk,
                                      art_node_t *new_child) {
    uint8_t val_idx = node->keys[key_chunk];
    assert(val_idx != ART_NODE48_EMPTY_VAL);
    node->children[val_idx] = new_child;
}

static inline art_indexed_child_t art_node48_next_child(
    const art_node48_t *node, int index) {
    art_indexed_child_t indexed_child;
    index++;
    for (size_t i = index; i < 256; ++i) {
        if (node->keys[i] != ART_NODE48_EMPTY_VAL) {
            indexed_child.child = node->children[node->keys[i]];
            indexed_child.index = i;
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static inline art_indexed_child_t art_node48_prev_child(
    const art_node48_t *node, int index) {
    if (index > 256) {
        index = 256;
    }
    index--;
    art_indexed_child_t indexed_child;
    for (int i = index; i >= 0; --i) {
        if (node->keys[i] != ART_NODE48_EMPTY_VAL) {
            indexed_child.child = node->children[node->keys[i]];
            indexed_child.index = i;
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static inline art_indexed_child_t art_node48_child_at(const art_node48_t *node,
                                                      int index) {
    art_indexed_child_t indexed_child;
    if (index < 0 || index >= 256) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[node->keys[index]];
    return indexed_child;
}

static inline art_indexed_child_t art_node48_lower_bound(
    art_node48_t *node, art_key_chunk_t key_chunk) {
    art_indexed_child_t indexed_child;
    for (size_t i = key_chunk; i < 256; ++i) {
        if (node->keys[i] != ART_NODE48_EMPTY_VAL) {
            indexed_child.index = i;
            indexed_child.child = node->children[node->keys[i]];
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static art_node256_t *art_node256_create(const art_key_chunk_t prefix[],
                                         uint8_t prefix_size) {
    art_node256_t *node =
        (art_node256_t *)roaring_malloc(sizeof(art_node256_t));
    art_init_inner_node(&node->base, ART_NODE256_TYPE, prefix, prefix_size);
    node->count = 0;
    for (size_t i = 0; i < 256; ++i) {
        node->children[i] = NULL;
    }
    return node;
}

static void art_free_node256(art_node256_t *node) {
    for (size_t i = 0; i < 256; ++i) {
        if (node->children[i] != NULL) {
            art_free_node(node->children[i]);
        }
    }
    roaring_free(node);
}

static inline art_node_t *art_node256_find_child(const art_node256_t *node,
                                                 art_key_chunk_t key) {
    return node->children[key];
}

static art_node_t *art_node256_insert(art_node256_t *node, art_node_t *child,
                                      uint8_t key) {
    node->children[key] = child;
    node->count++;
    return (art_node_t *)node;
}

static inline art_node_t *art_node256_erase(art_node256_t *node,
                                            uint8_t key_chunk) {
    node->children[key_chunk] = NULL;
    node->count--;
    if (node->count > 48) {
        return (art_node_t *)node;
    }

    art_node48_t *new_node =
        art_node48_create(node->base.prefix, node->base.prefix_size);
    for (size_t i = 0; i < 256; ++i) {
        if (node->children[i] != NULL) {
            art_node48_insert(new_node, node->children[i], i);
        }
    }
    roaring_free(node);
    return (art_node_t *)new_node;
}

static inline void art_node256_replace(art_node256_t *node,
                                       art_key_chunk_t key_chunk,
                                       art_node_t *new_child) {
    node->children[key_chunk] = new_child;
}

static inline art_indexed_child_t art_node256_next_child(
    const art_node256_t *node, int index) {
    art_indexed_child_t indexed_child;
    index++;
    for (size_t i = index; i < 256; ++i) {
        if (node->children[i] != NULL) {
            indexed_child.child = node->children[i];
            indexed_child.index = i;
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static inline art_indexed_child_t art_node256_prev_child(
    const art_node256_t *node, int index) {
    if (index > 256) {
        index = 256;
    }
    index--;
    art_indexed_child_t indexed_child;
    for (int i = index; i >= 0; --i) {
        if (node->children[i] != NULL) {
            indexed_child.child = node->children[i];
            indexed_child.index = i;
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

static inline art_indexed_child_t art_node256_child_at(
    const art_node256_t *node, int index) {
    art_indexed_child_t indexed_child;
    if (index < 0 || index >= 256) {
        indexed_child.child = NULL;
        return indexed_child;
    }
    indexed_child.index = index;
    indexed_child.child = node->children[index];
    return indexed_child;
}

static inline art_indexed_child_t art_node256_lower_bound(
    art_node256_t *node, art_key_chunk_t key_chunk) {
    art_indexed_child_t indexed_child;
    for (size_t i = key_chunk; i < 256; ++i) {
        if (node->children[i] != NULL) {
            indexed_child.index = i;
            indexed_child.child = node->children[i];
            return indexed_child;
        }
    }
    indexed_child.child = NULL;
    return indexed_child;
}

// Finds the child with the given key chunk in the inner node, returns NULL if
// no such child is found.
static art_node_t *art_find_child(const art_inner_node_t *node,
                                  art_key_chunk_t key_chunk) {
    switch (art_get_type(node)) {
        case ART_NODE4_TYPE:
            return art_node4_find_child((art_node4_t *)node, key_chunk);
        case ART_NODE16_TYPE:
            return art_node16_find_child((art_node16_t *)node, key_chunk);
        case ART_NODE48_TYPE:
            return art_node48_find_child((art_node48_t *)node, key_chunk);
        case ART_NODE256_TYPE:
            return art_node256_find_child((art_node256_t *)node, key_chunk);
        default:
            assert(false);
            return NULL;
    }
}

// Replaces the child with the given key chunk in the inner node.
static void art_replace(art_inner_node_t *node, art_key_chunk_t key_chunk,
                        art_node_t *new_child) {
    switch (art_get_type(node)) {
        case ART_NODE4_TYPE:
            art_node4_replace((art_node4_t *)node, key_chunk, new_child);
            break;
        case ART_NODE16_TYPE:
            art_node16_replace((art_node16_t *)node, key_chunk, new_child);
            break;
        case ART_NODE48_TYPE:
            art_node48_replace((art_node48_t *)node, key_chunk, new_child);
            break;
        case ART_NODE256_TYPE:
            art_node256_replace((art_node256_t *)node, key_chunk, new_child);
            break;
        default:
            assert(false);
    }
}

// Erases the child with the given key chunk from the inner node, returns the
// updated node (the same as the initial node if it was not shrunk).
static art_node_t *art_node_erase(art_inner_node_t *node,
                                  art_key_chunk_t key_chunk) {
    switch (art_get_type(node)) {
        case ART_NODE4_TYPE:
            return art_node4_erase((art_node4_t *)node, key_chunk);
        case ART_NODE16_TYPE:
            return art_node16_erase((art_node16_t *)node, key_chunk);
        case ART_NODE48_TYPE:
            return art_node48_erase((art_node48_t *)node, key_chunk);
        case ART_NODE256_TYPE:
            return art_node256_erase((art_node256_t *)node, key_chunk);
        default:
            assert(false);
            return NULL;
    }
}

// Inserts the leaf with the given key chunk in the inner node, returns a
// pointer to the (possibly expanded) node.
static art_node_t *art_node_insert_leaf(art_inner_node_t *node,
                                        art_key_chunk_t key_chunk,
                                        art_leaf_t *leaf) {
    art_node_t *child = (art_node_t *)(SET_LEAF(leaf));
    switch (art_get_type(node)) {
        case ART_NODE4_TYPE:
            return art_node4_insert((art_node4_t *)node, child, key_chunk);
        case ART_NODE16_TYPE:
            return art_node16_insert((art_node16_t *)node, child, key_chunk);
        case ART_NODE48_TYPE:
            return art_node48_insert((art_node48_t *)node, child, key_chunk);
        case ART_NODE256_TYPE:
            return art_node256_insert((art_node256_t *)node, child, key_chunk);
        default:
            assert(false);
            return NULL;
    }
}

// Frees the node and its children. Leaves are freed by the user.
static void art_free_node(art_node_t *node) {
    if (art_is_leaf(node)) {
        // We leave it up to the user to free leaves.
        return;
    }
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE:
            art_free_node4((art_node4_t *)node);
            break;
        case ART_NODE16_TYPE:
            art_free_node16((art_node16_t *)node);
            break;
        case ART_NODE48_TYPE:
            art_free_node48((art_node48_t *)node);
            break;
        case ART_NODE256_TYPE:
            art_free_node256((art_node256_t *)node);
            break;
        default:
            assert(false);
    }
}

// Returns the next child in key order, or NULL if called on a leaf.
// Provided index may be in the range [-1, 255].
static art_indexed_child_t art_node_next_child(const art_node_t *node,
                                               int index) {
    if (art_is_leaf(node)) {
        art_indexed_child_t indexed_child;
        indexed_child.child = NULL;
        return indexed_child;
    }
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE:
            return art_node4_next_child((art_node4_t *)node, index);
        case ART_NODE16_TYPE:
            return art_node16_next_child((art_node16_t *)node, index);
        case ART_NODE48_TYPE:
            return art_node48_next_child((art_node48_t *)node, index);
        case ART_NODE256_TYPE:
            return art_node256_next_child((art_node256_t *)node, index);
        default:
            assert(false);
            return (art_indexed_child_t){0};
    }
}

// Returns the previous child in key order, or NULL if called on a leaf.
// Provided index may be in the range [0, 256].
static art_indexed_child_t art_node_prev_child(const art_node_t *node,
                                               int index) {
    if (art_is_leaf(node)) {
        art_indexed_child_t indexed_child;
        indexed_child.child = NULL;
        return indexed_child;
    }
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE:
            return art_node4_prev_child((art_node4_t *)node, index);
        case ART_NODE16_TYPE:
            return art_node16_prev_child((art_node16_t *)node, index);
        case ART_NODE48_TYPE:
            return art_node48_prev_child((art_node48_t *)node, index);
        case ART_NODE256_TYPE:
            return art_node256_prev_child((art_node256_t *)node, index);
        default:
            assert(false);
            return (art_indexed_child_t){0};
    }
}

// Returns the child found at the provided index, or NULL if called on a leaf.
// Provided index is only valid if returned by art_node_(next|prev)_child.
static art_indexed_child_t art_node_child_at(const art_node_t *node,
                                             int index) {
    if (art_is_leaf(node)) {
        art_indexed_child_t indexed_child;
        indexed_child.child = NULL;
        return indexed_child;
    }
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE:
            return art_node4_child_at((art_node4_t *)node, index);
        case ART_NODE16_TYPE:
            return art_node16_child_at((art_node16_t *)node, index);
        case ART_NODE48_TYPE:
            return art_node48_child_at((art_node48_t *)node, index);
        case ART_NODE256_TYPE:
            return art_node256_child_at((art_node256_t *)node, index);
        default:
            assert(false);
            return (art_indexed_child_t){0};
    }
}

// Returns the child with the smallest key equal to or greater than the given
// key chunk, NULL if called on a leaf or no such child was found.
static art_indexed_child_t art_node_lower_bound(const art_node_t *node,
                                                art_key_chunk_t key_chunk) {
    if (art_is_leaf(node)) {
        art_indexed_child_t indexed_child;
        indexed_child.child = NULL;
        return indexed_child;
    }
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE:
            return art_node4_lower_bound((art_node4_t *)node, key_chunk);
        case ART_NODE16_TYPE:
            return art_node16_lower_bound((art_node16_t *)node, key_chunk);
        case ART_NODE48_TYPE:
            return art_node48_lower_bound((art_node48_t *)node, key_chunk);
        case ART_NODE256_TYPE:
            return art_node256_lower_bound((art_node256_t *)node, key_chunk);
        default:
            assert(false);
            return (art_indexed_child_t){0};
    }
}

// ====================== End of node-specific functions =======================

// Compares the given ranges of two keys, returns their relative order:
// * Key range 1 <  key range 2: a negative value
// * Key range 1 == key range 2: 0
// * Key range 1 >  key range 2: a positive value
static inline int art_compare_prefix(const art_key_chunk_t key1[],
                                     uint8_t key1_from,
                                     const art_key_chunk_t key2[],
                                     uint8_t key2_from, uint8_t length) {
    return memcmp(key1 + key1_from, key2 + key2_from, length);
}

// Compares two keys in full, see art_compare_prefix.
int art_compare_keys(const art_key_chunk_t key1[],
                     const art_key_chunk_t key2[]) {
    return art_compare_prefix(key1, 0, key2, 0, ART_KEY_BYTES);
}

// Returns the length of the common prefix between two key ranges.
static uint8_t art_common_prefix(const art_key_chunk_t key1[],
                                 uint8_t key1_from, uint8_t key1_to,
                                 const art_key_chunk_t key2[],
                                 uint8_t key2_from, uint8_t key2_to) {
    uint8_t min_len = key1_to - key1_from;
    uint8_t key2_len = key2_to - key2_from;
    if (key2_len < min_len) {
        min_len = key2_len;
    }
    uint8_t offset = 0;
    for (; offset < min_len; ++offset) {
        if (key1[key1_from + offset] != key2[key2_from + offset]) {
            return offset;
        }
    }
    return offset;
}

// Returns a pointer to the rootmost node where the value was inserted, may not
// be equal to `node`.
static art_node_t *art_insert_at(art_node_t *node, const art_key_chunk_t key[],
                                 uint8_t depth, art_leaf_t *new_leaf) {
    if (art_is_leaf(node)) {
        art_leaf_t *leaf = CAST_LEAF(node);
        uint8_t common_prefix = art_common_prefix(
            leaf->key, depth, ART_KEY_BYTES, key, depth, ART_KEY_BYTES);

        // Previously this was a leaf, create an inner node instead and add both
        // the existing and new leaf to it.
        art_node_t *new_node =
            (art_node_t *)art_node4_create(key + depth, common_prefix);

        new_node = art_node_insert_leaf((art_inner_node_t *)new_node,
                                        leaf->key[depth + common_prefix], leaf);
        new_node = art_node_insert_leaf((art_inner_node_t *)new_node,
                                        key[depth + common_prefix], new_leaf);

        // The new inner node is now the rootmost node.
        return new_node;
    }
    art_inner_node_t *inner_node = (art_inner_node_t *)node;
    // Not a leaf: inner node
    uint8_t common_prefix =
        art_common_prefix(inner_node->prefix, 0, inner_node->prefix_size, key,
                          depth, ART_KEY_BYTES);
    if (common_prefix != inner_node->prefix_size) {
        // Partial prefix match.  Create a new internal node to hold the common
        // prefix.
        art_node4_t *node4 =
            art_node4_create(inner_node->prefix, common_prefix);

        // Make the existing internal node a child of the new internal node.
        node4 = (art_node4_t *)art_node4_insert(
            node4, node, inner_node->prefix[common_prefix]);

        // Correct the prefix of the moved internal node, trimming off the chunk
        // inserted into the new internal node.
        inner_node->prefix_size = inner_node->prefix_size - common_prefix - 1;
        if (inner_node->prefix_size > 0) {
            // Move the remaining prefix to the correct position.
            memmove(inner_node->prefix, inner_node->prefix + common_prefix + 1,
                    inner_node->prefix_size);
        }

        // Insert the value in the new internal node.
        return art_node_insert_leaf(&node4->base, key[common_prefix + depth],
                                    new_leaf);
    }
    // Prefix matches entirely or node has no prefix. Look for an existing
    // child.
    art_key_chunk_t key_chunk = key[depth + common_prefix];
    art_node_t *child = art_find_child(inner_node, key_chunk);
    if (child != NULL) {
        art_node_t *new_child =
            art_insert_at(child, key, depth + common_prefix + 1, new_leaf);
        if (new_child != child) {
            // Node type changed.
            art_replace(inner_node, key_chunk, new_child);
        }
        return node;
    }
    return art_node_insert_leaf(inner_node, key_chunk, new_leaf);
}

// Erase helper struct.
typedef struct art_erase_result_s {
    // The rootmost node where the value was erased, may not be equal to `node`.
    // If no value was removed, this is null.
    art_node_t *rootmost_node;

    // Value removed, null if not removed.
    art_val_t *value_erased;
} art_erase_result_t;

// Searches for the given key starting at `node`, erases it if found.
static art_erase_result_t art_erase_at(art_node_t *node,
                                       const art_key_chunk_t *key,
                                       uint8_t depth) {
    art_erase_result_t result;
    result.rootmost_node = NULL;
    result.value_erased = NULL;

    if (art_is_leaf(node)) {
        art_leaf_t *leaf = CAST_LEAF(node);
        uint8_t common_prefix = art_common_prefix(leaf->key, 0, ART_KEY_BYTES,
                                                  key, 0, ART_KEY_BYTES);
        if (common_prefix != ART_KEY_BYTES) {
            // Leaf key mismatch.
            return result;
        }
        result.value_erased = (art_val_t *)leaf;
        return result;
    }
    art_inner_node_t *inner_node = (art_inner_node_t *)node;
    uint8_t common_prefix =
        art_common_prefix(inner_node->prefix, 0, inner_node->prefix_size, key,
                          depth, ART_KEY_BYTES);
    if (common_prefix != inner_node->prefix_size) {
        // Prefix mismatch.
        return result;
    }
    art_key_chunk_t key_chunk = key[depth + common_prefix];
    art_node_t *child = art_find_child(inner_node, key_chunk);
    if (child == NULL) {
        // No child with key chunk.
        return result;
    }
    // Try to erase the key further down. Skip the key chunk associated with the
    // child in the node.
    art_erase_result_t child_result =
        art_erase_at(child, key, depth + common_prefix + 1);
    if (child_result.value_erased == NULL) {
        return result;
    }
    result.value_erased = child_result.value_erased;
    result.rootmost_node = node;
    if (child_result.rootmost_node == NULL) {
        // Child node was fully erased, erase it from this node's children.
        result.rootmost_node = art_node_erase(inner_node, key_chunk);
    } else if (child_result.rootmost_node != child) {
        // Child node was not fully erased, update the pointer to it in this
        // node.
        art_replace(inner_node, key_chunk, child_result.rootmost_node);
    }
    return result;
}

// Searches for the given key starting at `node`, returns NULL if the key was
// not found.
static art_val_t *art_find_at(const art_node_t *node,
                              const art_key_chunk_t *key, uint8_t depth) {
    while (!art_is_leaf(node)) {
        art_inner_node_t *inner_node = (art_inner_node_t *)node;
        uint8_t common_prefix =
            art_common_prefix(inner_node->prefix, 0, inner_node->prefix_size,
                              key, depth, ART_KEY_BYTES);
        if (common_prefix != inner_node->prefix_size) {
            return NULL;
        }
        art_node_t *child =
            art_find_child(inner_node, key[depth + inner_node->prefix_size]);
        if (child == NULL) {
            return NULL;
        }
        node = child;
        // Include both the prefix and the child key chunk in the depth.
        depth += inner_node->prefix_size + 1;
    }
    art_leaf_t *leaf = CAST_LEAF(node);
    if (depth >= ART_KEY_BYTES - 1) {
        return (art_val_t *)leaf;
    }
    uint8_t common_prefix =
        art_common_prefix(leaf->key, 0, ART_KEY_BYTES, key, 0, ART_KEY_BYTES);
    if (common_prefix == ART_KEY_BYTES) {
        return (art_val_t *)leaf;
    }
    return NULL;
}

// Returns the size in bytes of the subtrie.
size_t art_size_in_bytes_at(const art_node_t *node) {
    if (art_is_leaf(node)) {
        return 0;
    }
    size_t size = 0;
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE: {
            size += sizeof(art_node4_t);
        } break;
        case ART_NODE16_TYPE: {
            size += sizeof(art_node16_t);
        } break;
        case ART_NODE48_TYPE: {
            size += sizeof(art_node48_t);
        } break;
        case ART_NODE256_TYPE: {
            size += sizeof(art_node256_t);
        } break;
        default:
            assert(false);
            break;
    }
    art_indexed_child_t indexed_child = art_node_next_child(node, -1);
    while (indexed_child.child != NULL) {
        size += art_size_in_bytes_at(indexed_child.child);
        indexed_child = art_node_next_child(node, indexed_child.index);
    }
    return size;
}

static void art_node_print_type(const art_node_t *node) {
    if (art_is_leaf(node)) {
        printf("Leaf");
        return;
    }
    switch (art_get_type((art_inner_node_t *)node)) {
        case ART_NODE4_TYPE:
            printf("Node4");
            return;
        case ART_NODE16_TYPE:
            printf("Node16");
            return;
        case ART_NODE48_TYPE:
            printf("Node48");
            return;
        case ART_NODE256_TYPE:
            printf("Node256");
            return;
        default:
            assert(false);
            return;
    }
}

void art_node_printf(const art_node_t *node, uint8_t depth) {
    printf("%*s", depth, "");
    printf("{\n");
    depth++;

    printf("%*s", depth, "");
    printf("type: ");
    art_node_print_type(node);
    printf("\n");

    if (art_is_leaf(node)) {
        art_leaf_t *leaf = CAST_LEAF(node);
        printf("%*s", depth, "");
        printf("key: ");
        for (size_t i = 0; i < ART_KEY_BYTES; ++i) {
            printf("%x", leaf->key[i]);
        }
        printf("\n");
        depth--;
        printf("%*s", depth, "");
        printf("}\n");
        return;
    }
    art_inner_node_t *inner_node = (art_inner_node_t *)node;
    printf("%*s", depth, "");
    printf("prefix_size: %d\n", inner_node->prefix_size);

    printf("%*s", depth, "");
    printf("prefix: ");
    for (uint8_t i = 0; i < inner_node->prefix_size; ++i) {
        printf("%x", (char)inner_node->prefix[i]);
    }
    printf("\n");

    switch (art_get_type(inner_node)) {
        case ART_NODE4_TYPE: {
            art_node4_t *node4 = (art_node4_t *)node;
            for (uint8_t i = 0; i < node4->count; ++i) {
                printf("%*s", depth, "");
                printf("key: %x\n", node4->keys[i]);
                art_node_printf(node4->children[i], depth);
            }
        } break;
        case ART_NODE16_TYPE: {
            art_node16_t *node16 = (art_node16_t *)node;
            for (uint8_t i = 0; i < node16->count; ++i) {
                printf("%*s", depth, "");
                printf("key: %x\n", node16->keys[i]);
                art_node_printf(node16->children[i], depth);
            }
        } break;
        case ART_NODE48_TYPE: {
            art_node48_t *node48 = (art_node48_t *)node;
            for (int i = 0; i < 256; ++i) {
                if (node48->keys[i] != ART_NODE48_EMPTY_VAL) {
                    printf("%*s", depth, "");
                    printf("key: %x\n", node48->keys[i]);
                    art_node_printf(node48->children[i], depth);
                }
            }
        } break;
        case ART_NODE256_TYPE: {
            art_node256_t *node256 = (art_node256_t *)node;
            for (int i = 0; i < 256; ++i) {
                if (node256->children[i] != NULL) {
                    printf("%*s", depth, "");
                    printf("key: %x\n", i);
                    art_node_printf(node256->children[i], depth);
                }
            }
        } break;
        default:
            assert(false);
            break;
    }
    depth--;
    printf("%*s", depth, "");
    printf("}\n");
}

void art_insert(art_t *art, const art_key_chunk_t *key, art_val_t *val) {
    art_leaf_t *leaf = (art_leaf_t *)val;
    art_leaf_populate(leaf, key);
    if (art->root == NULL) {
        art->root = (art_node_t *)SET_LEAF(leaf);
        return;
    }
    art->root = art_insert_at(art->root, key, 0, leaf);
}

art_val_t *art_erase(art_t *art, const art_key_chunk_t *key) {
    if (art->root == NULL) {
        return NULL;
    }
    art_erase_result_t result = art_erase_at(art->root, key, 0);
    if (result.value_erased == NULL) {
        return NULL;
    }
    art->root = result.rootmost_node;
    return result.value_erased;
}

art_val_t *art_find(const art_t *art, const art_key_chunk_t *key) {
    if (art->root == NULL) {
        return NULL;
    }
    return art_find_at(art->root, key, 0);
}

bool art_is_empty(const art_t *art) { return art->root == NULL; }

void art_free(art_t *art) {
    if (art->root == NULL) {
        return;
    }
    art_free_node(art->root);
}

size_t art_size_in_bytes(const art_t *art) {
    size_t size = sizeof(art_t);
    if (art->root != NULL) {
        size += art_size_in_bytes_at(art->root);
    }
    return size;
}

void art_printf(const art_t *art) {
    if (art->root == NULL) {
        return;
    }
    art_node_printf(art->root, 0);
}

// Returns the current node that the iterator is positioned at.
static inline art_node_t *art_iterator_node(art_iterator_t *iterator) {
    return iterator->frames[iterator->frame].node;
}

// Sets the iterator key and value to the leaf's key and value. Always returns
// true for convenience.
static inline bool art_iterator_valid_loc(art_iterator_t *iterator,
                                          art_leaf_t *leaf) {
    iterator->frames[iterator->frame].node = SET_LEAF(leaf);
    iterator->frames[iterator->frame].index_in_node = 0;
    memcpy(iterator->key, leaf->key, ART_KEY_BYTES);
    iterator->value = (art_val_t *)leaf;
    return true;
}

// Invalidates the iterator key and value. Always returns false for convenience.
static inline bool art_iterator_invalid_loc(art_iterator_t *iterator) {
    memset(iterator->key, 0, ART_KEY_BYTES);
    iterator->value = NULL;
    return false;
}

// Moves the iterator one level down in the tree, given a node at the current
// level and the index of the child that we're going down to.
//
// Note: does not set the index at the new level.
static void art_iterator_down(art_iterator_t *iterator,
                              const art_inner_node_t *node,
                              uint8_t index_in_node) {
    iterator->frames[iterator->frame].node = (art_node_t *)node;
    iterator->frames[iterator->frame].index_in_node = index_in_node;
    iterator->frame++;
    art_indexed_child_t indexed_child =
        art_node_child_at((art_node_t *)node, index_in_node);
    assert(indexed_child.child != NULL);
    iterator->frames[iterator->frame].node = indexed_child.child;
    iterator->depth += node->prefix_size + 1;
}

// Moves the iterator to the next/previous child of the current node. Returns
// the child moved to, or NULL if there is no neighboring child.
static art_node_t *art_iterator_neighbor_child(
    art_iterator_t *iterator, const art_inner_node_t *inner_node,
    bool forward) {
    art_iterator_frame_t frame = iterator->frames[iterator->frame];
    art_indexed_child_t indexed_child;
    if (forward) {
        indexed_child = art_node_next_child(frame.node, frame.index_in_node);
    } else {
        indexed_child = art_node_prev_child(frame.node, frame.index_in_node);
    }
    if (indexed_child.child != NULL) {
        art_iterator_down(iterator, inner_node, indexed_child.index);
    }
    return indexed_child.child;
}

// Moves the iterator one level up in the tree, returns false if not possible.
static bool art_iterator_up(art_iterator_t *iterator) {
    if (iterator->frame == 0) {
        return false;
    }
    iterator->frame--;
    // We went up, so we are at an inner node.
    iterator->depth -=
        ((art_inner_node_t *)art_iterator_node(iterator))->prefix_size + 1;
    return true;
}

// Initializes the iterator at the first / last leaf of the given node.
// Returns true for convenience.
static bool art_node_init_iterator(const art_node_t *node,
                                   art_iterator_t *iterator, bool first) {
    while (!art_is_leaf(node)) {
        art_indexed_child_t indexed_child;
        if (first) {
            indexed_child = art_node_next_child(node, -1);
        } else {
            indexed_child = art_node_prev_child(node, 256);
        }
        art_iterator_down(iterator, (art_inner_node_t *)node,
                          indexed_child.index);
        node = indexed_child.child;
    }
    // We're at a leaf.
    iterator->frames[iterator->frame].node = (art_node_t *)node;
    iterator->frames[iterator->frame].index_in_node = 0;  // Should not matter.
    return art_iterator_valid_loc(iterator, CAST_LEAF(node));
}

bool art_iterator_move(art_iterator_t *iterator, bool forward) {
    if (art_is_leaf(art_iterator_node(iterator))) {
        bool went_up = art_iterator_up(iterator);
        if (!went_up) {
            // This leaf is the root, we're done.
            return art_iterator_invalid_loc(iterator);
        }
    }
    // Advance within inner node.
    art_node_t *neighbor_child = art_iterator_neighbor_child(
        iterator, (art_inner_node_t *)art_iterator_node(iterator), forward);
    if (neighbor_child != NULL) {
        // There is another child at this level, go down to the first or last
        // leaf.
        return art_node_init_iterator(neighbor_child, iterator, forward);
    }
    // No more children at this level, go up.
    bool went_up = art_iterator_up(iterator);
    if (!went_up) {
        // We're at the root.
        return art_iterator_invalid_loc(iterator);
    }
    return art_iterator_move(iterator, forward);
}

// Assumes the iterator is positioned at a node with an equal prefix path up to
// the depth of the iterator.
static bool art_node_iterator_lower_bound(const art_node_t *node,
                                          art_iterator_t *iterator,
                                          const art_key_chunk_t key[]) {
    while (!art_is_leaf(node)) {
        art_inner_node_t *inner_node = (art_inner_node_t *)node;
        int prefix_comparison =
            art_compare_prefix(inner_node->prefix, 0, key, iterator->depth,
                               inner_node->prefix_size);
        if (prefix_comparison < 0) {
            // Prefix so far has been equal, but we've found a smaller key.
            // Since we take the lower bound within each node, we can return the
            // next leaf.
            return art_iterator_move(iterator, true);
        } else if (prefix_comparison > 0) {
            // No key equal to the key we're looking for, return the first leaf.
            return art_node_init_iterator(node, iterator, true);
        }
        // Prefix is equal, move to lower bound child.
        art_key_chunk_t key_chunk =
            key[iterator->depth + inner_node->prefix_size];
        art_indexed_child_t indexed_child =
            art_node_lower_bound(node, key_chunk);
        if (indexed_child.child == NULL) {
            // Only smaller keys among children.
            bool went_up = art_iterator_up(iterator);
            if (!went_up) {
                return art_iterator_invalid_loc(iterator);
            }
            return art_iterator_move(iterator, true);
        }
        // We found a child with a greater or equal prefix.
        art_iterator_down(iterator, inner_node, indexed_child.index);
        node = indexed_child.child;
    }
    art_leaf_t *leaf = CAST_LEAF(node);
    // Technically we don't have to re-compare the prefix if we arrived here
    // through the while loop, but it simplifies the code.
    if (art_compare_keys(leaf->key, key) >= 0) {
        return art_iterator_valid_loc(iterator, leaf);
    }
    return art_iterator_invalid_loc(iterator);
}

art_iterator_t art_init_iterator(const art_t *art, bool first) {
    art_iterator_t iterator = {0};
    if (art->root == NULL) {
        return iterator;
    }
    art_node_init_iterator(art->root, &iterator, first);
    return iterator;
}

bool art_iterator_next(art_iterator_t *iterator) {
    return art_iterator_move(iterator, true);
}

bool art_iterator_prev(art_iterator_t *iterator) {
    return art_iterator_move(iterator, false);
}

bool art_iterator_lower_bound(art_iterator_t *iterator,
                              const art_key_chunk_t *key) {
    int compare_result = art_compare_keys(iterator->key, key);
    // Move up until we have an equal or greater prefix, after which we can do a
    // normal lower bound search.
    while (compare_result < 0 && iterator->frame > 0) {
        if (!art_iterator_up(iterator)) {
            // Only smaller keys found.
            return art_node_iterator_lower_bound(art_iterator_node(iterator),
                                                 iterator, key);
        }
        // Since we're only moving up, we can keep comparing against the
        // iterator key.
        compare_result =
            art_compare_prefix(iterator->key, 0, key, 0, iterator->depth);
    }
    if (compare_result > 0) {
        return art_node_init_iterator(art_iterator_node(iterator), iterator,
                                      true);
    }
    return art_node_iterator_lower_bound(art_iterator_node(iterator), iterator,
                                         key);
}

art_iterator_t art_lower_bound(const art_t *art, const art_key_chunk_t *key) {
    art_iterator_t iterator = {0};
    if (art->root != NULL) {
        art_node_iterator_lower_bound(art->root, &iterator, key);
    }
    return iterator;
}

art_iterator_t art_upper_bound(const art_t *art, const art_key_chunk_t *key) {
    art_iterator_t iterator = {0};
    if (art->root != NULL) {
        if (art_node_iterator_lower_bound(art->root, &iterator, key) &&
            art_compare_keys(iterator.key, key) == 0) {
            art_iterator_next(&iterator);
        }
    }
    return iterator;
}

void art_iterator_insert(art_t *art, art_iterator_t *iterator,
                         const art_key_chunk_t *key, art_val_t *val) {
    // TODO: This can likely be faster.
    art_insert(art, key, val);
    assert(art->root != NULL);
    iterator->frame = 0;
    iterator->depth = 0;
    art_node_iterator_lower_bound(art->root, iterator, key);
}

// TODO: consider keeping `art_t *art` in the iterator.
art_val_t *art_iterator_erase(art_t *art, art_iterator_t *iterator) {
    if (iterator->value == NULL) {
        return NULL;
    }
    art_val_t *value_erased = iterator->value;
    bool went_up = art_iterator_up(iterator);
    if (!went_up) {
        art->root = NULL;
        art_iterator_invalid_loc(iterator);
        return value_erased;
    }

    // Erase the leaf.
    art_node_t *child_to_replace;
    {
        art_inner_node_t *node =
            (art_inner_node_t *)art_iterator_node(iterator);
        art_key_chunk_t key_chunk =
            iterator->key[iterator->depth + node->prefix_size];
        child_to_replace = art_node_erase(node, key_chunk);
    }

    // Replace the pointer to the inner node we erased from in its parent (it
    // may be a leaf now).
    went_up = art_iterator_up(iterator);
    if (went_up) {
        art_inner_node_t *node =
            (art_inner_node_t *)art_iterator_node(iterator);
        art_key_chunk_t key_chunk =
            iterator->key[iterator->depth + node->prefix_size];
        art_replace(node, key_chunk, child_to_replace);
    } else {
        // This node was the rootmost node.
        art->root = child_to_replace;
        iterator->frames[0].node = child_to_replace;
    }
    art_key_chunk_t initial_key[ART_KEY_BYTES];
    memcpy(initial_key, iterator->key, ART_KEY_BYTES);
    // Search for the first key after the one we erased.
    art_iterator_lower_bound(iterator, initial_key);
    return value_erased;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace roaring
}  // namespace internal
#endif

