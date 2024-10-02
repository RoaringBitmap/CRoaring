// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "roaring/roaring.h"
#include "roaring/roaring64.h"

int bitmap32(const char *data, size_t size) {
    // We test that deserialization never fails.
    roaring_bitmap_t *bitmap =
        roaring_bitmap_portable_deserialize_safe(data, size);
    if (bitmap) {
        // The bitmap may not be usable if it does not follow the specification.
        // We can validate the bitmap we recovered to make sure it is proper.
        const char *reason_failure = NULL;
        if (roaring_bitmap_internal_validate(bitmap, &reason_failure)) {
            // the bitmap is ok!
            uint32_t cardinality = roaring_bitmap_get_cardinality(bitmap);

            for (uint32_t i = 100; i < 1000; i++) {
                if (!roaring_bitmap_contains(bitmap, i)) {
                    cardinality++;
                    roaring_bitmap_add(bitmap, i);
                }
            }
            uint32_t new_cardinality = roaring_bitmap_get_cardinality(bitmap);
            if (cardinality != new_cardinality) {
                printf("bug\n");
                exit(1);
            }
        }
        roaring_bitmap_free(bitmap);
    }
    return 0;
}

int bitmap64(const char *data, size_t size) {
    // We test that deserialization never fails.
    roaring64_bitmap_t *bitmap =
        roaring64_bitmap_portable_deserialize_safe(data, size);
    if (bitmap) {
        // The bitmap may not be usable if it does not follow the specification.
        // We can validate the bitmap we recovered to make sure it is proper.
        const char *reason_failure = NULL;
        if (roaring64_bitmap_internal_validate(bitmap, &reason_failure)) {
            // the bitmap is ok!
            uint64_t cardinality = roaring64_bitmap_get_cardinality(bitmap);

            for (uint32_t i = 100; i < 1000; i++) {
                if (!roaring64_bitmap_contains(bitmap, i)) {
                    cardinality++;
                    roaring64_bitmap_add(bitmap, i);
                }
            }
            uint64_t new_cardinality = roaring64_bitmap_get_cardinality(bitmap);
            if (cardinality != new_cardinality) {
                printf("bug\n");
                exit(1);
            }
        }
        roaring64_bitmap_free(bitmap);
    }
    return 0;
}

int LLVMFuzzerTestOneInput(const char *data, size_t size) {
    if (size == 0) {
        return 0;
    }
    if (data[0] % 2 == 0) {
        return bitmap32(data + 1, size - 1);
    } else {
        return bitmap64(data + 1, size - 1);
    }
}
