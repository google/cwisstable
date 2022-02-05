#!/bin/env python3
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Simple script for generating the extract.h header's boilerplate.
"""

DEPTH = 64 
KEYS = [
  'obj_copy', 'obj_dtor',
  'key_hash', 'key_eq',
  'alloc_alloc', 'alloc_free',
  
  'slot_size', 'slot_align', 'slot_init',
  'slot_transfer', 'slot_get', 'slot_dtor',
]

def main():
  for key in KEYS:
    print(f'#define CWISS_EXTRACT_{key}(key, val) CWISS_EXTRACT_{key}Z##key')
    print(f'#define CWISS_EXTRACT_{key}Z{key} CWISS_NOTHING, CWISS_NOTHING, CWISS_NOTHING')
  print()

  for i in range(0, DEPTH):
    print(f'#define CWISS_EXTRACT{i:02X}(needle, kv, ...) CWISS_SELECT{i:02X}(needle kv, CWISS_EXTRACT_VALUE, kv, CWISS_EXTRACT{i+1:02X}, (needle, __VA_ARGS__), CWISS_NOTHING)')
  print()
  for i in range(0, DEPTH):
    print(f'#define CWISS_SELECT{i:02X}(x, ...) CWISS_SELECT{i:02X}_(x, __VA_ARGS__)')
  print()
  for i in range(0, DEPTH):
    print(f'#define CWISS_SELECT{i:02X}_(_1, _2, _3, call, args, ...) call args')
  print()
  print('#endif  // CWISSTABLE_EXTRACT_H_')

if __name__ == '__main__': main()