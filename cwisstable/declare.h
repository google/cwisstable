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

#ifndef CWISSTABLE_DECLARE_H_
#define CWISSTABLE_DECLARE_H_

#include <stdbool.h>
#include <stddef.h>

#include "cwisstable/base.h"
#include "cwisstable/policy.h"
#include "cwisstable/raw_hash_set.h"

// Specialized HashSet/Map declaration macros.

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

#define CWISS_DECLARE_FLAT_HASHSET(HashSet_, Type_)         \
  CWISS_DECLARE_POD_FLAT_POLICY(HashSet_##_kPolicy, Type_); \
  CWISS_DECLARE_HASHSET_WITH(HashSet_, Type_, HashSet_##_kPolicy)

#define CWISS_DECLARE_NODE_HASHSET(HashSet_, Type_)         \
  CWISS_DECLARE_POD_NODE_POLICY(HashSet_##_kPolicy, Type_); \
  CWISS_DECLARE_HASHSET_WITH(HashSet_, Type_, HashSet_##_kPolicy)

#define CWISS_DECLARE_HASHSET_WITH(HashSet_, Type_, kPolicy_) \
  typedef Type_ HashMap_##_Entry;                             \
  CWISS_DECLARE_COMMON_(HashSet_, HashMap_##_Entry, HashMap_##_Entry, kPolicy_)

#define CWISS_DECLARE_FLAT_HASHMAP(HashMap_, K_, V_)             \
  CWISS_DECLARE_POD_FLAT_MAP_POLICY(HashMap_##_kPolicy, K_, V_); \
  CWISS_DECLARE_HASHMAP_WITH(HashMap_, K_, V_, HashMap_##_kPolicy)

#define CWISS_DECLARE_NODE_HASHMAP(HashMap_, K_, V_)             \
  CWISS_DECLARE_POD_NODE_MAP_POLICY(HashMap_##_kPolicy, K_, V_); \
  CWISS_DECLARE_HASHMAP_WITH(HashMap_, K_, V_, HashMap_##_kPolicy)

#define CWISS_DECLARE_HASHMAP_WITH(HashMap_, K_, V_, kPolicy_) \
  typedef struct {                                             \
    K_ key;                                                    \
    V_ val;                                                    \
  } HashMap_##_Entry;                                          \
  typedef K_ HashMap_##_Key;                                   \
  typedef V_ HashMap_##_Value;                                 \
  CWISS_DECLARE_COMMON_(HashMap_, HashMap_##_Entry, HashMap_##_Key, kPolicy_)

#define CWISS_DECLARE_COMMON_(HashSet_, Type_, Key_, kPolicy_)                 \
  CWISS_BEGIN_                                                                 \
  static inline const CWISS_Policy* HashSet_##_policy() { return &kPolicy_; }  \
                                                                               \
  typedef struct {                                                             \
    CWISS_RawHashSet set_;                                                     \
  } HashSet_;                                                                  \
  static inline void HashSet_##_dump(const HashSet_* self) {                   \
    CWISS_RawHashSet_dump(&kPolicy_, &self->set_);                             \
  }                                                                            \
                                                                               \
  static inline HashSet_ HashSet_##_new(size_t bucket_count) {                 \
    return (HashSet_){CWISS_RawHashSet_new(&kPolicy_, bucket_count)};          \
  }                                                                            \
  static inline HashSet_ HashSet_##_dup(const HashSet_* that) {                \
    return (HashSet_){CWISS_RawHashSet_dup(&kPolicy_, &that->set_)};           \
  }                                                                            \
  static inline void HashSet_##_destroy(HashSet_* self) {                      \
    CWISS_RawHashSet_destroy(&kPolicy_, &self->set_);                          \
  }                                                                            \
                                                                               \
  typedef struct {                                                             \
    CWISS_RawIter it_;                                                         \
  } HashSet_##_Iter;                                                           \
  static inline HashSet_##_Iter HashSet_##_iter(HashSet_* self) {              \
    return (HashSet_##_Iter){CWISS_RawHashSet_iter(&kPolicy_, &self->set_)};   \
  }                                                                            \
  static inline Type_* HashSet_##_Iter_get(const HashSet_##_Iter* it) {        \
    return (Type_*)CWISS_RawIter_get(&kPolicy_, &it->it_);                     \
  }                                                                            \
  static inline Type_* HashSet_##_Iter_next(HashSet_##_Iter* it) {             \
    return (Type_*)CWISS_RawIter_next(&kPolicy_, &it->it_);                    \
  }                                                                            \
                                                                               \
  typedef struct {                                                             \
    CWISS_RawIter it_;                                                         \
  } HashSet_##_CIter;                                                          \
  static inline HashSet_##_CIter HashSet_##_citer(const HashSet_* self) {      \
    return (HashSet_##_CIter){CWISS_RawHashSet_citer(&kPolicy_, &self->set_)}; \
  }                                                                            \
  static inline const Type_* HashSet_##_CIter_get(const HashSet_##_Iter* it) { \
    return (const Type_*)CWISS_RawIter_get(&kPolicy_, &it->it_);               \
  }                                                                            \
  static inline const Type_* HashSet_##_CIter_next(HashSet_##_Iter* it) {      \
    return (const Type_*)CWISS_RawIter_next(&kPolicy_, &it->it_);              \
  }                                                                            \
  static inline HashSet_##_CIter HashSet_##_Iter_const(HashSet_##_Iter it) {   \
    return (HashSet_##_CIter){it.it_};                                         \
  }                                                                            \
                                                                               \
  static inline void HashSet_##_reserve(HashSet_* self, size_t n) {            \
    CWISS_RawHashSet_reserve(&kPolicy_, &self->set_, n);                       \
  }                                                                            \
  static inline void HashSet_##_rehash(HashSet_* self, size_t n) {             \
    CWISS_RawHashSet_rehash(&kPolicy_, &self->set_, n);                        \
  }                                                                            \
                                                                               \
  static inline bool HashSet_##_empty(const HashSet_* self) {                  \
    return CWISS_RawHashSet_empty(&kPolicy_, &self->set_);                     \
  }                                                                            \
  static inline size_t HashSet_##_size(const HashSet_* self) {                 \
    return CWISS_RawHashSet_size(&kPolicy_, &self->set_);                      \
  }                                                                            \
  static inline size_t HashSet_##_capacity(const HashSet_* self) {             \
    return CWISS_RawHashSet_capacity(&kPolicy_, &self->set_);                  \
  }                                                                            \
                                                                               \
  static inline void HashSet_##_clear(HashSet_* self) {                        \
    return CWISS_RawHashSet_clear(&kPolicy_, &self->set_);                     \
  }                                                                            \
                                                                               \
  typedef struct {                                                             \
    HashSet_##_Iter iter;                                                      \
    bool inserted;                                                             \
  } HashSet_##_Insert;                                                         \
  static inline HashSet_##_Insert HashSet_##_insert(HashSet_* self,            \
                                                    Type_* val) {              \
    CWISS_Insert ret = CWISS_RawHashSet_insert(&kPolicy_, &self->set_, val);   \
    return (HashSet_##_Insert){{ret.iter}, ret.inserted};                      \
  }                                                                            \
                                                                               \
  static inline HashSet_##_CIter HashSet_##_cfind_hinted(                      \
      const HashSet_* self, const Type_* key, size_t hash) {                   \
    return (HashSet_##_CIter){                                                 \
        CWISS_RawHashSet_find_hinted(&kPolicy_, &self->set_, key, hash)};      \
  }                                                                            \
  static inline HashSet_##_Iter HashSet_##_find_hinted(                        \
      HashSet_* self, const Key_* key, size_t hash) {                          \
    return (HashSet_##_Iter){                                                  \
        CWISS_RawHashSet_find_hinted(&kPolicy_, &self->set_, key, hash)};      \
  }                                                                            \
  static inline HashSet_##_CIter HashSet_##_cfind(const HashSet_* self,        \
                                                  const Key_* key) {           \
    return (HashSet_##_CIter){                                                 \
        CWISS_RawHashSet_find(&kPolicy_, &self->set_, key)};                   \
  }                                                                            \
  static inline HashSet_##_Iter HashSet_##_find(HashSet_* self,                \
                                                const Key_* key) {             \
    return (HashSet_##_Iter){                                                  \
        CWISS_RawHashSet_find(&kPolicy_, &self->set_, key)};                   \
  }                                                                            \
                                                                               \
  static inline bool HashSet_##_contains(const HashSet_* self,                 \
                                         const Key_* key) {                    \
    return CWISS_RawHashSet_contains(&kPolicy_, &self->set_, key);             \
  }                                                                            \
                                                                               \
  static inline void HashSet_##_erase_at(HashSet_##_Iter it) {                 \
    CWISS_RawHashSet_erase_at(&kPolicy_, it.it_);                              \
  }                                                                            \
  static inline void HashSet_##_erase(HashSet_* self, const Key_* key) {       \
    CWISS_RawHashSet_erase(&kPolicy_, &self->set_, key);                       \
  }                                                                            \
                                                                               \
  CWISS_END_                                                                   \
  /* Force a semicolon. */ struct HashSet_##_NeedsTrailingSemicolon_ { int x; }

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_DECLARE_H_