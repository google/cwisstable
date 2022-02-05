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

#include "cwisstable/internal/base.h"
#include "cwisstable/internal/raw_hash_set.h"
#include "cwisstable/policy.h"

/// SwissTable code generation macros.
///
/// This file is the entry-point for users of `cwisstable`. It exports six
/// macros for generating different kinds of tables. Four correspond to Abseil's
/// four SwissTable containers:
///
/// - `CWISS_DECLARE_FLAT_HASHSET(Set, Type)`
/// - `CWISS_DECLARE_FLAT_HASHMAP(Map, Key, Value)`
/// - `CWISS_DECLARE_NODE_HASHSET(Set, Type)`
/// - `CWISS_DECLARE_NODE_HASHMAP(Map, Key, Value)`
///
/// These expand to a type (with the same name as the first argument) and and
/// a collection of strongly-typed functions associated to it (the generated
/// API is described below). These macros use the default policy (see policy.h)
/// for each of the four containers; custom policies may be used instead via
/// the following macros:
///
/// - `CWISS_DECLARE_HASHSET_WITH(Set, Type, kPolicy)`
/// - `CWISS_DECLARE_HASHMAP_WITH(Map, Key, Value, kPolicy)`
///
/// `kPolicy` must be a constant global variable referring to an appropriate
/// property for the element types of the container.
///
/// The generated API is safe: the functions are well-typed and automatically
/// pass the correct policy pointer. Because the pointer is a constant
/// expression, it promotes devirtualization when inlining.
///
/// # Generated API
///
/// The set and map macros all generate the same API (respectively), so we
/// describe only the API of the flat versions.
///
/// Note that every every function that does not take `self` by const pointer
/// is permitted to trigger a rehash and invalidate iterators, unless otherwise
/// noted.
///
/// ```
/// // CWISS_DECLARE_FLAT_HASHSET(MySet, T);
///
/// /// Returns the policy used with this set type.
/// static inline const CWISS_Policy* MySet_policy();
///
/// /// The generated type.
/// typedef struct MySet;
///
/// /// Constructs a new set with the given initial capacity.
/// static inline MySet MySet_new(size_t capacity);
///
/// /// Creates a deep copy of this set.
/// static inline MySet MySet_dup(const MySet* self);
///
/// /// Destroys this set.
/// static inline void MySet_destroy(const MySet* self);
///
/// /// Dumps the internal contents of the table to stderr; intended only for
/// /// debugging.
/// ///
/// /// The output of this function is not stable.
/// static inline void MySet_dump(const MySet* self);
///
/// /// Ensures that there is at least `n` spare capacity, potentially resizing
/// /// if necessary.
/// static inline void MySet_reserve(MySet* self, size_t n);
///
/// /// Resizes the table to have at least `n` buckets of capacity.
/// static inline void MySet_rehash(MySet* self, size_t n);
///
/// /// Returns whether the set is empty.
/// static inline size_t MySet_empty(const MySet* self);
///
/// /// Returns the number of elements stored in the table.
/// static inline size_t MySet_size(const MySet* self);
///
/// /// Returns the number of buckets in the table.
/// ///
/// /// Note that this is *different* from the amount of elements that must be
/// /// in the table before a resize is triggered.
/// static inline size_t MySet_capacity(const MySet* self);
///
/// /// Erases every element in the set.
/// static inline void MySet_clear(MySet* self);
///
/// /// A non-mutating iterator into a `MySet`.
/// typedef struct MySet_CIter;
///
/// /// Creates a new non-mutating iterator fro this table.
/// static inline MySet_CIter MySet_citer(const MySet* self);
///
/// /// Returns a pointer to the element this iterator is at; returns `NULL` if
/// /// this iterator has reached the end of the table.
/// static inline const T* MySet_CIter_get(const MySet_CIter* it);
///
/// /// Advances this iterator, returning a pointer to the element the iterator
/// /// winds up pointing to (see `MySet_CIter_get()`).
/// ///
/// /// The iterator must not point to the end of the table.
/// static inline const T* MySet_CIter_next(const MySet_CIter* it);
///
/// /// A mutating iterator into a `MySet`.
/// typedef struct MySet_Iter;
///
/// /// Creates a new mutating iterator fro this table.
/// static inline MySet_Iter MySet_iter(const MySet* self);
///
/// /// Returns a pointer to the element this iterator is at; returns `NULL` if
/// /// this iterator has reached the end of the table.
/// static inline T* MySet_Iter_get(const MySet_Iter* it);
///
/// /// Advances this iterator, returning a pointer to the element the iterator
/// /// winds up pointing to (see `MySet_Iter_get()`).
/// ///
/// /// The iterator must not point to the end of the table.
/// static inline T* MySet_Iter_next(const MySet_Iter* it);
///
/// /// Checks if this set contains the given element.
/// ///
/// /// In general, if you plan to use the element and not just check for it,
/// /// prefer `MySet_find()` and friends.
/// static inline bool MySet_contains(const MySet* self, const T* key);
///
/// /// Searches the table for `key`, non-mutating iterator version.
/// ///
/// /// If found, returns an iterator at the found element; otherwise, returns
/// /// an iterator that's already at the end: `get()` will return `NULL`.
/// static inline MySet_CIter MySet_cfind(const MySet* self, const T* key);
///
/// /// Searches the table for `key`, mutating iterator version.
/// ///
/// /// If found, returns an iterator at the found element; otherwise, returns
/// /// an iterator that's already at the end: `get()` will return `NULL`.
/// ///
/// /// This function does not trigger rehashes.
/// static inline MySet_Iter MySet_find(MySet* self, const T* key);
///
/// /// Like `MySet_cfind`, but takes a pre-computed hash.
/// ///
/// /// The hash must be correct for `key`.
/// static inline MySet_CIter MySet_cfind_hinted(const MySet* self,
///                                              const T* key, size_t hash);
///
/// /// Like `MySet_find`, but takes a pre-computed hash.
/// ///
/// /// The hash must be correct for `key`.
/// ///
/// /// This function does not trigger rehashes.
/// static inline MySet_Iter MySet_find_hinted(MySet* self, const T* key,
///                                            size_t hash);
///
/// /// The return type of `MySet_insert()`.
/// typedef struct {
///   MySet_Iter iter;
///   bool inserted;
/// } MySet_Insert;
///
/// /// Inserts `val` into the set if it isn't already present. Returns an
/// /// iterator pointing to the element in the set and whether it was just
/// /// inserted or was already present.
/// static inline MySet_Insert MySet_insert(MySet* self, T* val);
///
/// /// Erases (and destroys) the element pointed to by `it`.
/// ///
/// /// Although the iterator doesn't point to anything now, this function does
/// /// not trigger rehashes and the erased iterator can still be safely
/// /// advanced (although not dereferenced until advanced).
/// static inline void MySet_erase_at(MySet_Iter it);
///
/// /// Looks up `key` and erases it from the set.
/// ///
/// /// Returns `true` if erasure happened.
/// static inline bool MySet_erase(MySet* self, const T* key);
/// ```
///
/// ```
/// // CWISS_DECLARE_FLAT_HASHMAP(MyMap, K, V);
///
/// /// Returns the policy used with this map type.
/// static inline const CWISS_Policy* MyMap_policy();
///
/// /// The generated type.
/// typedef struct { /* ... */ } MyMap;
///
/// /// A key-value pair in the map.
/// typedef struct {
///   K key;
///   V val;
/// } MyMap_Entry;
///
/// /// Constructs a new map with the given initial capacity.
/// static inline MyMap MyMap_new(size_t capacity);
///
/// /// Creates a deep copy of this map.
/// static inline MyMap MyMap_dup(const MyMap* self);
///
/// /// Destroys this map.
/// static inline void MyMap_destroy(const MyMap* self);
///
/// /// Dumps the internal contents of the table to stderr; intended only for
/// /// debugging.
/// ///
/// /// The output of this function is not stable.
/// static inline void MyMap_dump(const MyMap* self);
///
/// /// Ensures that there is at least `n` spare capacity, potentially resizing
/// /// if necessary.
/// static inline void MyMap_reserve(MyMap* self, size_t n);
///
/// /// Resizes the table to have at least `n` buckets of capacity.
/// static inline void MyMap_rehash(MyMap* self, size_t n);
///
/// /// Returns whether the map is empty.
/// static inline size_t MyMap_empty(const MyMap* self);
///
/// /// Returns the number of elements stored in the table.
/// static inline size_t MyMap_size(const MyMap* self);
///
/// /// Returns the number of buckets in the table.
/// ///
/// /// Note that this is *different* from the amount of elements that must be
/// /// in the table before a resize is triggered.
/// static inline size_t MyMap_capacity(const MyMap* self);
///
/// /// Erases every element in the map.
/// static inline void MyMap_clear(MyMap* self);
///
/// /// A non-mutating iterator into a `MyMap`.
/// typedef struct { /* ... */ } MyMap_CIter;
///
/// /// Creates a new non-mutating iterator fro this table.
/// static inline MyMap_CIter MyMap_citer(const MyMap* self);
///
/// /// Returns a pointer to the element this iterator is at; returns `NULL` if
/// /// this iterator has reached the end of the table.
/// static inline const MyMap_Entry* MyMap_CIter_get(const MyMap_CIter* it);
///
/// /// Advances this iterator, returning a pointer to the element the iterator
/// /// winds up pointing to (see `MyMap_CIter_get()`).
/// ///
/// /// The iterator must not point to the end of the table.
/// static inline const MyMap_Entry* MyMap_CIter_next(const MyMap_CIter* it);
///
/// /// A mutating iterator into a `MyMap`.
/// typedef struct { /* ... */ } MyMap_Iter;
///
/// /// Creates a new mutating iterator fro this table.
/// static inline MyMap_Iter MyMap_iter(const MyMap* self);
///
/// /// Returns a pointer to the element this iterator is at; returns `NULL` if
/// /// this iterator has reached the end of the table.
/// static inline MyMap_Entry* MyMap_Iter_get(const MyMap_Iter* it);
///
/// /// Advances this iterator, returning a pointer to the element the iterator
/// /// winds up pointing to (see `MyMap_Iter_get()`).
/// ///
/// /// The iterator must not point to the end of the table.
/// static inline MyMap_Entry* MyMap_Iter_next(const MyMap_Iter* it);
///
/// /// Checks if this map contains the given element.
/// ///
/// /// In general, if you plan to use the element and not just check for it,
/// /// prefer `MyMap_find()` and friends.
/// static inline bool MyMap_contains(const MyMap* self, const K* key);
///
/// /// Searches the table for `key`, non-mutating iterator version.
/// ///
/// /// If found, returns an iterator at the found element; otherwise, returns
/// /// an iterator that's already at the end: `get()` will return `NULL`.
/// static inline MyMap_CIter MyMap_cfind(const MyMap* self, const K* key);
///
/// /// Searches the table for `key`, mutating iterator version.
/// ///
/// /// If found, returns an iterator at the found element; otherwise, returns
/// /// an iterator that's already at the end: `get()` will return `NULL`.
/// ///
/// /// This function does not trigger rehashes.
/// static inline MyMap_Iter MyMap_find(MyMap* self, const K* key);
///
/// /// Like `MyMap_cfind`, but takes a pre-computed hash.
/// ///
/// /// The hash must be correct for `key`.
/// static inline MyMap_CIter MyMap_cfind_hinted(const MyMap* self,
///                                              const K* key, size_t hash);
///
/// /// Like `MyMap_find`, but takes a pre-computed hash.
/// ///
/// /// The hash must be correct for `key`.
/// ///
/// /// This function does not trigger rehashes.
/// static inline MyMap_Iter MyMap_find_hinted(MyMap* self, const K* key,
///                                            size_t hash);
///
/// /// The return type of `MyMap_insert()`.
/// typedef struct {
///   MyMap_Iter iter;
///   bool inserted;
/// } MyMap_Insert;
///
/// /// Inserts `val` into the map if it isn't already present. Returns an
/// /// iterator pointing to the element in the map and whether it was just
/// /// inserted or was already present.
/// static inline MyMap_Insert MyMap_insert(MyMap* self, MyMap_Entry* val);
///
/// /// Erases (and destroys) the element pointed to by `it`.
/// ///
/// /// Although the iterator doesn't point to anything now, this function does
/// /// not trigger rehashes and the erased iterator can still be safely
/// /// advanced (although not dereferenced until advanced).
/// static inline void MyMap_erase_at(MyMap_Iter it);
///
/// /// Looks up `key` and erases it from the map.
/// ///
/// /// Returns `true` if erasure happened.
/// static inline bool MyMap_erase(MyMap* self, const K* key);
/// ```

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

/// Generates a new hash set type with inline storage and the default
/// plain-old-data policies.
///
/// See header documentation for examples of generated API.
#define CWISS_DECLARE_FLAT_HASHSET(HashSet_, Type_)                 \
  CWISS_DECLARE_FLAT_SET_POLICY(HashSet_##_kPolicy, Type_, (_, _)); \
  CWISS_DECLARE_HASHSET_WITH(HashSet_, Type_, HashSet_##_kPolicy)

/// Generates a new hash set type with outline storage and the default
/// plain-old-data policies.
///
/// See header documentation for examples of generated API.
#define CWISS_DECLARE_NODE_HASHSET(HashSet_, Type_)                 \
  CWISS_DECLARE_NODE_SET_POLICY(HashSet_##_kPolicy, Type_, (_, _)); \
  CWISS_DECLARE_HASHSET_WITH(HashSet_, Type_, HashSet_##_kPolicy)

/// Generates a new hash map type with inline storage and the default
/// plain-old-data policies.
///
/// See header documentation for examples of generated API.
#define CWISS_DECLARE_FLAT_HASHMAP(HashMap_, K_, V_)                 \
  CWISS_DECLARE_FLAT_MAP_POLICY(HashMap_##_kPolicy, K_, V_, (_, _)); \
  CWISS_DECLARE_HASHMAP_WITH(HashMap_, K_, V_, HashMap_##_kPolicy)

/// Generates a new hash map type with outline storage and the default
/// plain-old-data policies.
///
/// See header documentation for examples of generated API.
#define CWISS_DECLARE_NODE_HASHMAP(HashMap_, K_, V_)                 \
  CWISS_DECLARE_NODE_MAP_POLICY(HashMap_##_kPolicy, K_, V_, (_, _)); \
  CWISS_DECLARE_HASHMAP_WITH(HashMap_, K_, V_, HashMap_##_kPolicy)

/// Generates a new hash set type using the given policy.
///
/// See header documentation for examples of generated API.
#define CWISS_DECLARE_HASHSET_WITH(HashSet_, Type_, kPolicy_) \
  typedef Type_ HashSet_##_Entry;                             \
  CWISS_DECLARE_COMMON_(HashSet_, HashSet_##_Entry, HashSet_##_Entry, kPolicy_)

/// Generates a new hash map type using the given policy.
///
/// See header documentation for examples of generated API.
#define CWISS_DECLARE_HASHMAP_WITH(HashMap_, K_, V_, kPolicy_) \
  typedef struct {                                             \
    K_ key;                                                    \
    V_ val;                                                    \
  } HashMap_##_Entry;                                          \
  typedef K_ HashMap_##_Key;                                   \
  typedef V_ HashMap_##_Value;                                 \
  CWISS_DECLARE_COMMON_(HashMap_, HashMap_##_Entry, HashMap_##_Key, kPolicy_)

// ---- PUBLIC API ENDS HERE! ----

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
  static inline bool HashSet_##_erase(HashSet_* self, const Key_* key) {       \
    return CWISS_RawHashSet_erase(&kPolicy_, &self->set_, key);                \
  }                                                                            \
                                                                               \
  CWISS_END_                                                                   \
  /* Force a semicolon. */ struct HashSet_##_NeedsTrailingSemicolon_ { int x; }

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_DECLARE_H_