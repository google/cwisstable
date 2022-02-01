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

#ifndef CWISSTABLE_POLICY_H_
#define CWISSTABLE_POLICY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cwisstable/base.h"

// Table policies, which define how a particular kind of table operates on the
// objects it tracks.

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

typedef size_t CWISS_FxHash_State;
static inline void CWISS_FxHash_Write(CWISS_FxHash_State* state,
                                      const void* val, size_t len) {
  const size_t kSeed = (size_t)(UINT64_C(0x517cc1b727220a95));
  const uint32_t kRotate = 5;

  const char* p = (const char*)val;
  CWISS_FxHash_State state_ = *state;
  while (len > 0) {
    size_t word = 0;
    size_t to_read = len >= sizeof(state_) ? sizeof(state_) : len;
    memcpy(&word, p, to_read);

    state_ = (state_ << kRotate) | (state_ >> (sizeof(state_) * 8 - kRotate));
    state_ ^= word;
    state_ *= kSeed;

    len -= to_read;
    p += to_read;
  }
  *state = state_;
}

typedef struct {
  // The size and alignment of the stored object.
  size_t size, align;

  // Copies an object.
  void (*copy)(void* dst, const void* src);

  // Destroys an object.
  //
  // This function may, as an optimization, be null. This will cause it to
  // behave as a no-op.
  void (*dtor)(void* val);
} CWISS_ObjectPolicy;

#define CWISS_DECLARE_POD_OBJECT(kPolicy_, Type_)                  \
  static inline void kPolicy_##_copy(void* dst, const void* src) { \
    memcpy(dst, src, sizeof(Type_));                               \
  }                                                                \
  static const CWISS_ObjectPolicy kPolicy_ = {                     \
      sizeof(Type_),                                               \
      alignof(Type_),                                              \
      kPolicy_##_copy,                                             \
      NULL,                                                        \
  }

#define CWISS_DECLARE_OBJECT_WITH(kPolicy_, Type_, copy_, dtor_)         \
  static inline void kPolicy_##_copy_value(void* dst, const void* src) { \
    memcpy(dst, src, sizeof(Type_));                                     \
  }                                                                      \
  static const CWISS_ObjectPolicy kPolicy_ = {                           \
      sizeof(Type_),                                                     \
      alignof(Type_),                                                    \
      copy_,                                                             \
      dtor_,                                                             \
  }

typedef struct {
  // Hashes a value.
  //
  // This function must be such that if two elements compare equal, they must
  // have the same hash (but not vice-versa).
  size_t (*hash)(const void* val);

  // Compares two values for equality.
  bool (*eq)(const void* a, const void* b);
} CWISS_KeyPolicy;

#define CWISS_DECLARE_POD_SET_KEY(kPolicy_, Type_) \
  CWISS_DECLARE_POD_MAP_KEY(kPolicy_, Type_, Type_)

#define CWISS_DECLARE_POD_MAP_KEY(kPolicy_, Type_, K_)             \
  static inline size_t kPolicy_##_hash(const void* val) {          \
    CWISS_FxHash_State state = 0;                                  \
    CWISS_FxHash_Write(&state, val, sizeof(K_));                   \
    return state;                                                  \
  }                                                                \
  static inline bool kPolicy_##_eq(const void* a, const void* b) { \
    return memcmp(a, b, sizeof(K_)) == 0;                          \
  }                                                                \
  static const CWISS_KeyPolicy kPolicy_ = {                        \
      kPolicy_##_hash,                                             \
      kPolicy_##_eq,                                               \
  }

typedef struct {
  // Allocates memory for use by the table's backing array.
  //
  // Allocators must never fail and never return null, unlike `malloc`. This
  // function does not need to tolerate zero sized allocations.
  void* (*alloc)(size_t size, size_t align);

  // Deallocates memory allocated by `alloc`.
  //
  // This function is passed the same size/alignment as was passed to `alloc`,
  // allowing for sized-delete optimizations.
  void (*free)(void* array, size_t size, size_t align);
} CWISS_AllocPolicy;

static inline void* CWISS_DefaultMalloc(size_t size, size_t align) {
  void* p = malloc(size);  // TODO: Check alignment.
  CWISS_CHECK(p != NULL, "malloc() returned null");
  return p;
}
static inline void CWISS_DefaultFree(void* array, size_t size, size_t align) {
  free(array);
}

static const CWISS_AllocPolicy CWISS_kDefaultAlloc = {CWISS_DefaultMalloc,
                                                      CWISS_DefaultFree};

typedef struct {
  // The size and alignment of the slot stored in the backing array of the
  // table. For example, in a map, this might be a pair, while in a node-based
  // structure, it could just be a pointer.
  size_t size, align;

  // Constructs a new slot, for placing a value into.
  void (*init)(void* slot);
  // Destroys a slot, including the destruction of the value it contains.
  //
  // This function may, as an optimization, be null. This will cause it to
  // behave as a no-op.
  void (*del)(void* slot);
  // Transfers a slot.
  //
  // `dst` must be uninitialized; `src` must be initialized. After this
  // function, their roles will be switched.
  void (*transfer)(void* dst, void* src);
  // Extracts an element out of `slot`, returning a pointer to it.
  //
  // This function does not need to tolerate nulls.
  void* (*get)(void* slot);
} CWISS_SlotPolicy;

#define CWISS_DECLARE_FLAT_SLOT_POLICY(kPolicy_, kObject_, Type_)           \
  static inline void kPolicy_##_slot_init(void* slot) {}                    \
  static inline void kPolicy_##_slot_transfer(void* dst, void* src) {       \
    memcpy(dst, src, kObject_.size);                                        \
  }                                                                         \
  static inline void* kPolicy_##_slot_get(void* slot) { return slot; }      \
  static inline void kPolicy_##_slot_dtor(void* slot) {                     \
    if (kObject_.dtor != NULL) {                                            \
      kObject_.dtor(slot);                                                  \
    }                                                                       \
  }                                                                         \
  static const CWISS_SlotPolicy kPolicy_ = {                                \
      sizeof(Type_),        alignof(Type_),           kPolicy_##_slot_init, \
      kPolicy_##_slot_dtor, kPolicy_##_slot_transfer, kPolicy_##_slot_get,  \
  }

#define CWISS_DECLARE_NODE_SLOT_POLICY(kPolicy_, kObject_, Type_)          \
  static inline void kPolicy_##_slot_init(void* slot) {                    \
    void* node = CWISS_DefaultMalloc(kObject_.size, kObject_.align);       \
    memcpy(slot, &node, sizeof(node));                                     \
  }                                                                        \
  static inline void kPolicy_##_slot_del(void* slot) {                     \
    if (kObject_.dtor != NULL) {                                           \
      kObject_.dtor(*((void**)slot));                                      \
    }                                                                      \
    CWISS_DefaultFree(*(void**)slot, kObject_.size, kObject_.align);       \
  }                                                                        \
  static inline void kPolicy_##_slot_transfer(void* dst, void* src) {      \
    memcpy(dst, src, sizeof(void*));                                       \
  }                                                                        \
  static inline void* kPolicy_##_slot_get(void* slot) {                    \
    return *((void**)slot);                                                \
  }                                                                        \
  static const CWISS_SlotPolicy kPolicy_ = {                               \
      sizeof(void*),       alignof(void*),           kPolicy_##_slot_init, \
      kPolicy_##_slot_del, kPolicy_##_slot_transfer, kPolicy_##_slot_get,  \
  }

// A hash table policy.
//
// Policies define how elements are manipulated inside of a hash table, allowing
// for fine control over storage, allocating, hashing, and comparison
// strategies.
typedef struct {
  const CWISS_ObjectPolicy* obj;
  const CWISS_KeyPolicy* key;
  const CWISS_AllocPolicy* alloc;
  const CWISS_SlotPolicy* slot;
} CWISS_Policy;

// TODO(#5): Provide better macros for generating policies for complex types
// like heap-allocated character buffers (e.g. C-style strings).

#define CWISS_DECLARE_POD_FLAT_POLICY(kPolicy_, Type_)            \
  CWISS_DECLARE_POD_OBJECT(kPolicy_##_ObjectPolicy, Type_);       \
  CWISS_DECLARE_POD_SET_KEY(kPolicy_##_KeyPolicy, Type_);         \
  CWISS_DECLARE_FLAT_SLOT_POLICY(kPolicy_##_SlotPolicy,           \
                                 kPolicy_##_ObjectPolicy, Type_); \
  static const CWISS_Policy kPolicy_ = {                          \
      &kPolicy_##_ObjectPolicy,                                   \
      &kPolicy_##_KeyPolicy,                                      \
      &CWISS_kDefaultAlloc,                                       \
      &kPolicy_##_SlotPolicy,                                     \
  }

#define CWISS_DECLARE_POD_FLAT_MAP_POLICY(kPolicy_, K_, V_)                  \
  typedef struct {                                                           \
    K_ k;                                                                    \
    V_ V;                                                                    \
  } kPolicy_##_Entry;                                                        \
  CWISS_DECLARE_POD_OBJECT(kPolicy_##_ObjectPolicy, kPolicy_##_Entry);       \
  CWISS_DECLARE_POD_MAP_KEY(kPolicy_##_KeyPolicy, kPolicy_##_Entry, K_);     \
  CWISS_DECLARE_FLAT_SLOT_POLICY(kPolicy_##_SlotPolicy,                      \
                                 kPolicy_##_ObjectPolicy, kPolicy_##_Entry); \
  static const CWISS_Policy kPolicy_ = {                                     \
      &kPolicy_##_ObjectPolicy,                                              \
      &kPolicy_##_KeyPolicy,                                                 \
      &CWISS_kDefaultAlloc,                                                  \
      &kPolicy_##_SlotPolicy,                                                \
  }

#define CWISS_DECLARE_POD_NODE_POLICY(kPolicy_, Type_)            \
  CWISS_DECLARE_POD_OBJECT(kPolicy_##_ObjectPolicy, Type_);       \
  CWISS_DECLARE_POD_KEY(kPolicy_##_KeyPolicy, Type_);             \
  CWISS_DECLARE_NODE_SLOT_POLICY(kPolicy_##_SlotPolicy,           \
                                 kPolicy_##_ObjectPolicy, Type_); \
  static const CWISS_Policy kPolicy_ = {                          \
      &kPolicy_##_ObjectPolicy,                                   \
      &kPolicy_##_KeyPolicy,                                      \
      &CWISS_kDefaultAlloc,                                       \
      &kPolicy_##_SlotPolicy,                                     \
  }

#define CWISS_DECLARE_POD_NODE_MAP_POLICY(kPolicy_, K_, V_)                  \
  typedef struct {                                                           \
    K_ k;                                                                    \
    V_ V;                                                                    \
  } kPolicy_##_Entry;                                                        \
  CWISS_DECLARE_POD_OBJECT(kPolicy_##_ObjectPolicy, kPolicy_##_Entry);       \
  CWISS_DECLARE_POD_MAP_KEY(kPolicy_##_KeyPolicy, kPolicy_##_Entry, K_);     \
  CWISS_DECLARE_NODE_SLOT_POLICY(kPolicy_##_SlotPolicy,                      \
                                 kPolicy_##_ObjectPolicy, kPolicy_##_Entry); \
  static const CWISS_Policy kPolicy_ = {                                     \
      &kPolicy_##_ObjectPolicy,                                              \
      &kPolicy_##_KeyPolicy,                                                 \
      &CWISS_kDefaultAlloc,                                                  \
      &kPolicy_##_SlotPolicy,                                                \
  }

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_POLICY_H_