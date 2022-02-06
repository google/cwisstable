// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CWISSTABLE_TEST_HELPERS_H_
#define CWISSTABLE_TEST_HELPERS_H_

// Helpers for interacting with C SwissTables from C++.

#include "cwisstable.h"

#include <cstdint>

namespace cwisstable {
template <typename T>
struct DefaultHash {
  size_t operator()(const T& val) {
    CWISS_FxHash_State state = 0;
    CWISS_FxHash_Write(&state, &val, sizeof(T));
    return state;
  }
};

template <typename T>
struct DefaultEq {
  bool operator()(const T& a, const T& b) { return a == b; }
};

template <typename T, typename Hash, typename Eq>
struct FlatPolicyWrapper {
  CWISS_GCC_PUSH_
  CWISS_GCC_ALLOW_("-Waddress")
  // clang-format off
  CWISS_DECLARE_FLAT_SET_POLICY(kPolicy, T,
    (modifiers, static constexpr),
    (obj_copy, [](void* dst, const void* src) {
      new (dst) T(*static_cast<const T*>(src));
    }),
    (obj_dtor, [](void* val) {
      static_cast<T*>(val)->~T();
    }),
    (key_hash, [](const void* val) {
      return Hash{}(*static_cast<const T*>(val));
    }),
    (key_eq, [](const void* a, const void* b) {
      return Eq{}(*static_cast<const T*>(a), *static_cast<const T*>(b));
    }),
    (slot_transfer, [](void* dst, void* src) {
      T* old = static_cast<T*>(src);
      new (dst) T(std::move(*old));
      old->~T();
    }));
  // clang-format on
  CWISS_GCC_POP_
};

template <typename T, typename Hash = DefaultHash<T>,
          typename Eq = DefaultEq<T>>
constexpr const CWISS_Policy& FlatPolicy() {
  return FlatPolicyWrapper<T, Hash, Eq>::kPolicy;
}


// Helpers for doing some operations on tables with minimal pain.
//
// This macro expands to functions that will form an overload set with other
// table types.
#define TABLE_HELPERS(HashSet_)                                              \
  HashSet_##_Entry* Find(HashSet_& set, const HashSet_##_Key& needle) {      \
    auto it = HashSet_##_find(&set, &needle);                                \
    return HashSet_##_Iter_get(&it);                                         \
  }                                                                          \
  std::pair<HashSet_##_Entry*, bool> Insert(HashSet_& set,                   \
                                            const HashSet_##_Entry& value) { \
    auto it = HashSet_##_insert(&set, &value);                               \
    return {HashSet_##_Iter_get(&it.iter), it.inserted};                     \
  }                                                                          \
  bool Erase(HashSet_& set, const HashSet_##_Key& needle) {                  \
    return HashSet_##_erase(&set, &needle);                                  \
  }                                                                          \
  std::vector<HashSet_##_Entry> Collect(const HashSet_& set) {               \
    std::vector<HashSet_##_Entry> items;                                     \
    items.reserve(HashSet_##_size(&set));                                    \
    for (auto it = HashSet_##_citer(&set); HashSet_##_CIter_get(&it);        \
         HashSet_##_CIter_next(&it)) {                                       \
      items.push_back(*HashSet_##_CIter_get(&it));                           \
    }                                                                        \
    return items;                                                            \
  }
}

#endif  // CWISSTABLE_TEST_HELPERS_H_
