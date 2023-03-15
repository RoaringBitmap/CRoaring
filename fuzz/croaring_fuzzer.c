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
#include <string.h>
#include <stdlib.h>
#include "roaring/roaring.h"

int LLVMFuzzerTestOneInput(const char *data, size_t size) {
    // We test that deserialization never fails.
    roaring_bitmap_t* bitmap = roaring_bitmap_portable_deserialize_safe(data, size);
    if(bitmap) {
        // The bitmap may not be usable if it does not follow the specification.
        roaring_bitmap_free(bitmap);
    }
    return 0;
}
