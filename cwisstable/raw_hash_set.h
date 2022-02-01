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

#ifndef CWISSTABLE_RAW_HASH_SET_H_
#define CWISSTABLE_RAW_HASH_SET_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cwisstable/base.h"
#include "cwisstable/bits.h"
#include "cwisstable/capacity.h"
#include "cwisstable/ctrl.h"
#include "cwisstable/policy.h"
#include "cwisstable/probe.h"

// The core SwissTable implementation.

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

typedef struct {
  CWISS_ctrl_t* ctrl_;  // [(capacity + 1 + NumClonedBytes()) * ctrl_t]
  char* slots_;         // [capacity * slot_type]
  size_t size_;         // number of full slots
  size_t capacity_;     // total number of slots
  size_t growth_left_;
} CWISS_RawHashSet;

static inline void CWISS_RawHashSet_dump(const CWISS_Policy* policy,
                                         const CWISS_RawHashSet* self) {
  fprintf(stderr, "%p / %zu / %zu\n", self->ctrl_, self->size_,
          self->capacity_);
  if (self->capacity_ == 0) {
    return;
  }

  for (size_t i = 0; i <= self->capacity_; ++i) {
    fprintf(stderr, "[%4zu] %p / ", i, &self->ctrl_[i]);
    switch (self->ctrl_[i]) {
      case CWISS_kSentinel:
        fprintf(stderr, "kSentinel: //\n");
        continue;
      case CWISS_kEmpty:
        fprintf(stderr, "   kEmpty");
        break;
      case CWISS_kDeleted:
        fprintf(stderr, " kDeleted");
        break;
      default:
        fprintf(stderr, " H2(0x%02x)", self->ctrl_[i]);
        break;
    }

    char* slot = self->slots_ + i * policy->slot->size;
    fprintf(stderr, ": %p /", slot);
    for (size_t j = 0; j < policy->slot->size; ++j) {
      fprintf(stderr, " %02x", (unsigned char)slot[j]);
    }
    char* elem = (char*)policy->slot->get(slot);
    if (elem != slot && CWISS_IsFull(self->ctrl_[i])) {
      fprintf(stderr, " ->");
      for (size_t j = 0; j < policy->obj->size; ++j) {
        fprintf(stderr, " %02x", (unsigned char)elem[j]);
      }
    }
    fprintf(stderr, "\n");
  }
}

typedef struct {
  CWISS_RawHashSet* set_;
  CWISS_ctrl_t* ctrl_;
  char* slot_;
} CWISS_RawIter;

static inline void CWISS_RawIter_skip_empty_or_deleted(
    const CWISS_Policy* policy, CWISS_RawIter* self) {
  while (CWISS_IsEmptyOrDeleted(*self->ctrl_)) {
    CWISS_Group g = CWISS_Group_new(self->ctrl_);
    uint32_t shift = CWISS_Group_CountLeadingEmptyOrDeleted(&g);
    self->ctrl_ += shift;
    self->slot_ += shift * policy->slot->size;
  }

  // Not sure why this is a branch rather than a cmov; Abseil uses a branch.
  if (CWISS_UNLIKELY(*self->ctrl_ == CWISS_kSentinel)) {
    self->ctrl_ = NULL;
    self->slot_ = NULL;
  }
}

static inline CWISS_RawIter CWISS_RawHashSet_iter_at(const CWISS_Policy* policy,
                                                     CWISS_RawHashSet* self,
                                                     size_t index) {
  CWISS_RawIter iter = {
      self,
      self->ctrl_ + index,
      self->slots_ + index * policy->slot->size,
  };
  CWISS_RawIter_skip_empty_or_deleted(policy, &iter);
  CWISS_AssertIsFull(iter.ctrl_);
  return iter;
}

static inline CWISS_RawIter CWISS_RawHashSet_iter(const CWISS_Policy* policy,
                                                  CWISS_RawHashSet* self) {
  return CWISS_RawHashSet_iter_at(policy, self, 0);
}

static inline CWISS_RawIter CWISS_RawHashSet_citer_at(
    const CWISS_Policy* policy, const CWISS_RawHashSet* self, size_t index) {
  return CWISS_RawHashSet_iter_at(policy, (CWISS_RawHashSet*)self, index);
}

static inline CWISS_RawIter CWISS_RawHashSet_citer(
    const CWISS_Policy* policy, const CWISS_RawHashSet* self) {
  return CWISS_RawHashSet_iter(policy, (CWISS_RawHashSet*)self);
}

static inline void* CWISS_RawIter_get(const CWISS_Policy* policy,
                                      const CWISS_RawIter* self) {
  CWISS_AssertIsValid(self->ctrl_);
  if (self->slot_ == NULL) {
    return NULL;
  }

  return policy->slot->get(self->slot_);
}

static inline void* CWISS_RawIter_next(const CWISS_Policy* policy,
                                       CWISS_RawIter* self) {
  CWISS_AssertIsFull(self->ctrl_);
  ++self->ctrl_;
  self->slot_ += policy->slot->size;

  CWISS_RawIter_skip_empty_or_deleted(policy, self);
  return CWISS_RawIter_get(policy, self);
}

// "erases" the object from the container, except that it doesn't actually
// destroy the object. It only updates all the metadata of the class.
// This can be used in conjunction with Policy::transfer to move the object to
// another place.
static inline void CWISS_RawHashSet_erase_meta_only(const CWISS_Policy* policy,
                                                    CWISS_RawIter it) {
  CWISS_DCHECK(CWISS_IsFull(*it.ctrl_), "erasing a dangling iterator");
  --it.set_->size_;
  const size_t index = (size_t)(it.ctrl_ - it.set_->ctrl_);
  const size_t index_before = (index - CWISS_Group_kWidth) & it.set_->capacity_;
  CWISS_Group g_after = CWISS_Group_new(it.ctrl_);
  CWISS_BitMask empty_after = CWISS_Group_MatchEmpty(&g_after);
  CWISS_Group g_before = CWISS_Group_new(it.set_->ctrl_ + index_before);
  CWISS_BitMask empty_before = CWISS_Group_MatchEmpty(&g_before);

  // We count how many consecutive non empties we have to the right and to the
  // left of `it`. If the sum is >= kWidth then there is at least one probe
  // window that might have seen a full group.
  bool was_never_full =
      empty_before.mask && empty_after.mask &&
      (size_t)(CWISS_BitMask_TrailingZeros(&empty_after) +
               CWISS_BitMask_LeadingZeros(&empty_before)) < CWISS_Group_kWidth;

  CWISS_SetCtrl(index, was_never_full ? CWISS_kEmpty : CWISS_kDeleted,
                it.set_->capacity_, it.set_->ctrl_, it.set_->slots_,
                policy->slot->size);
  it.set_->growth_left_ += was_never_full;
  // infoz().RecordErase();
}

static inline void CWISS_RawHashSet_reset_growth_left(
    const CWISS_Policy* policy, CWISS_RawHashSet* self) {
  self->growth_left_ = CWISS_CapacityToGrowth(self->capacity_) - self->size_;
}

static inline void CWISS_RawHashSet_initialize_slots(const CWISS_Policy* policy,
                                                     CWISS_RawHashSet* self) {
  CWISS_DCHECK(self->capacity_, "capacity should be nonzero");
  // Folks with custom allocators often make unwarranted assumptions about the
  // behavior of their classes vis-a-vis trivial destructability and what
  // calls they will or wont make.  Avoid sampling for people with custom
  // allocators to get us out of this mess.  This is not a hard guarantee but
  // a workaround while we plan the exact guarantee we want to provide.
  //
  // People are often sloppy with the exact type of their allocator (sometimes
  // it has an extra const or is missing the pair, but rebinds made it work
  // anyway).  To avoid the ambiguity, we work off SlotAlloc which we have
  // bound more carefully.
  //
  // NOTE(mcyoung): Not relevant in C but kept in case we decide to do custom
  // alloc.
  /*if (std::is_same<SlotAlloc, std::allocator<slot_type>>::value &&
      slots_ == nullptr) {
    infoz() = Sample(sizeof(slot_type));
  }*/

  char* mem =
      (char*)  // Cast for C++.
      policy->alloc->alloc(CWISS_AllocSize(self->capacity_, policy->slot->size,
                                           policy->slot->align),
                           policy->slot->align);

  self->ctrl_ = (CWISS_ctrl_t*)mem;
  self->slots_ = mem + CWISS_SlotOffset(self->capacity_, policy->slot->align);
  CWISS_ResetCtrl(self->capacity_, self->ctrl_, self->slots_,
                  policy->slot->size);
  CWISS_RawHashSet_reset_growth_left(policy, self);

  // infoz().RecordStorageChanged(size_, capacity_);
}

static inline void CWISS_RawHashSet_destroy_slots(const CWISS_Policy* policy,
                                                  CWISS_RawHashSet* self) {
  if (!self->capacity_) return;

  if (policy->slot->del != NULL) {
    for (size_t i = 0; i != self->capacity_; ++i) {
      if (CWISS_IsFull(self->ctrl_[i])) {
        policy->slot->del(self->slots_ + i * policy->slot->size);
      }
    }
  }

  policy->alloc->free(
      self->ctrl_,
      CWISS_AllocSize(self->capacity_, policy->slot->size, policy->slot->align),
      policy->slot->align);
  self->ctrl_ = CWISS_EmptyGroup();
  self->slots_ = NULL;
  self->size_ = 0;
  self->capacity_ = 0;
  self->growth_left_ = 0;
}

static inline void CWISS_RawHashSet_resize(const CWISS_Policy* policy,
                                           CWISS_RawHashSet* self,
                                           size_t new_capacity) {
  CWISS_DCHECK(CWISS_IsValidCapacity(new_capacity), "invalid capacity: %zu",
               new_capacity);
  CWISS_ctrl_t* old_ctrl = self->ctrl_;
  char* old_slots = self->slots_;
  const size_t old_capacity = self->capacity_;
  self->capacity_ = new_capacity;
  CWISS_RawHashSet_initialize_slots(policy, self);

  size_t total_probe_length = 0;
  for (size_t i = 0; i != old_capacity; ++i) {
    if (CWISS_IsFull(old_ctrl[i])) {
      size_t hash = policy->key->hash(
          policy->slot->get(old_slots + i * policy->slot->size));
      CWISS_FindInfo target =
          CWISS_find_first_non_full(self->ctrl_, hash, self->capacity_);
      size_t new_i = target.offset;
      total_probe_length += target.probe_length;
      CWISS_SetCtrl(new_i, CWISS_H2(hash), self->capacity_, self->ctrl_,
                    self->slots_, policy->slot->size);
      policy->slot->transfer(self->slots_ + new_i * policy->slot->size,
                             old_slots + i * policy->slot->size);
    }
  }
  if (old_capacity) {
    // TODO(#6): Implement MSAN support.
    // SanitizerUnpoisonMemoryRegion(old_slots,
    //                               sizeof(slot_type) * old_capacity);
    policy->alloc->free(
        old_ctrl,
        CWISS_AllocSize(old_capacity, policy->slot->size, policy->slot->align),
        policy->slot->align);
  }
  // infoz().RecordRehash(total_probe_length);
}

CWISS_NOINLINE static void CWISS_RawHashSet_drop_deletes_without_resize(
    const CWISS_Policy* policy, CWISS_RawHashSet* self) {
  CWISS_DCHECK(CWISS_IsValidCapacity(self->capacity_), "invalid capacity: %zu",
               self->capacity_);
  CWISS_DCHECK(!CWISS_is_small(self->capacity_),
               "unexpected small capacity: %zu", self->capacity_);
  // Algorithm:
  // - mark all DELETED slots as EMPTY
  // - mark all FULL slots as DELETED
  // - for each slot marked as DELETED
  //     hash = Hash(element)
  //     target = find_first_non_full(hash)
  //     if target is in the same group
  //       mark slot as FULL
  //     else if target is EMPTY
  //       transfer element to target
  //       mark slot as EMPTY
  //       mark target as FULL
  //     else if target is DELETED
  //       swap current element with target element
  //       mark target as FULL
  //       repeat procedure for current slot with moved from element (target)
  CWISS_ConvertDeletedToEmptyAndFullToDeleted(self->ctrl_, self->capacity_);
  size_t total_probe_length = 0;
  // Unfortunately because we do not know this size statically, we need to take
  // a trip to the allocator. Alternatively we could use a variable length
  // alloca...
  void* slot = policy->alloc->alloc(policy->slot->size, policy->slot->align);

  for (size_t i = 0; i != self->capacity_; ++i) {
    if (!CWISS_IsDeleted(self->ctrl_[i])) continue;

    char* old_slot = self->slots_ + i * policy->slot->size;
    size_t hash = policy->key->hash(policy->slot->get(old_slot));

    const CWISS_FindInfo target =
        CWISS_find_first_non_full(self->ctrl_, hash, self->capacity_);
    const size_t new_i = target.offset;
    total_probe_length += target.probe_length;

    char* new_slot = self->slots_ + new_i * policy->slot->size;

    // Verify if the old and new i fall within the same group wrt the hash.
    // If they do, we don't need to move the object as it falls already in the
    // best probe we can.
    const size_t probe_offset =
        CWISS_probe(self->ctrl_, hash, self->capacity_).offset_;
#define CWISS_probe_index(pos) \
  (((pos - probe_offset) & self->capacity_) / CWISS_Group_kWidth)

    // Element doesn't move.
    if (CWISS_LIKELY(CWISS_probe_index(new_i) == CWISS_probe_index(i))) {
      CWISS_SetCtrl(i, CWISS_H2(hash), self->capacity_, self->ctrl_,
                    self->slots_, policy->slot->size);
      continue;
    }
    if (CWISS_IsEmpty(self->ctrl_[new_i])) {
      // Transfer element to the empty spot.
      // SetCtrl poisons/unpoisons the slots so we have to call it at the
      // right time.
      CWISS_SetCtrl(new_i, CWISS_H2(hash), self->capacity_, self->ctrl_,
                    self->slots_, policy->slot->size);
      policy->slot->transfer(new_slot, old_slot);
      CWISS_SetCtrl(i, CWISS_kEmpty, self->capacity_, self->ctrl_, self->slots_,
                    policy->slot->size);
    } else {
      CWISS_DCHECK(CWISS_IsDeleted(self->ctrl_[new_i]),
                   "bad ctrl value at %zu: %02x", new_i, self->ctrl_[new_i]);
      CWISS_SetCtrl(new_i, CWISS_H2(hash), self->capacity_, self->ctrl_,
                    self->slots_, policy->slot->size);
      // Until we are done rehashing, DELETED marks previously FULL slots.
      // Swap i and new_i elements.

      policy->slot->transfer(slot, old_slot);
      policy->slot->transfer(old_slot, new_slot);
      policy->slot->transfer(new_slot, slot);
      --i;  // repeat
    }
#undef CWISS_probe_index
  }
  CWISS_RawHashSet_reset_growth_left(policy, self);
  policy->alloc->free(slot, policy->slot->size, policy->slot->align);
  // infoz().RecordRehash(total_probe_length);
}

static inline void CWISS_RawHashSet_rehash_and_grow_if_necessary(
    const CWISS_Policy* policy, CWISS_RawHashSet* self) {
  if (self->capacity_ == 0) {
    CWISS_RawHashSet_resize(policy, self, 1);
  } else if (self->capacity_ > CWISS_Group_kWidth &&
             // Do these calcuations in 64-bit to avoid overflow.
             self->size_ * UINT64_C(32) <= self->capacity_ * UINT64_C(25)) {
    // Squash DELETED without growing if there is enough capacity.
    //
    // Rehash in place if the current size is <= 25/32 of capacity_.
    // Rationale for such a high factor: 1) drop_deletes_without_resize() is
    // faster than resize, and 2) it takes quite a bit of work to add
    // tombstones.  In the worst case, seems to take approximately 4
    // insert/erase pairs to create a single tombstone and so if we are
    // rehashing because of tombstones, we can afford to rehash-in-place as
    // long as we are reclaiming at least 1/8 the capacity without doing more
    // than 2X the work.  (Where "work" is defined to be size() for rehashing
    // or rehashing in place, and 1 for an insert or erase.)  But rehashing in
    // place is faster per operation than inserting or even doubling the size
    // of the table, so we actually afford to reclaim even less space from a
    // resize-in-place.  The decision is to rehash in place if we can reclaim
    // at about 1/8th of the usable capacity (specifically 3/28 of the
    // capacity) which means that the total cost of rehashing will be a small
    // fraction of the total work.
    //
    // Here is output of an experiment using the BM_CacheInSteadyState
    // benchmark running the old case (where we rehash-in-place only if we can
    // reclaim at least 7/16*capacity_) vs. this code (which rehashes in place
    // if we can recover 3/32*capacity_).
    //
    // Note that although in the worst-case number of rehashes jumped up from
    // 15 to 190, but the number of operations per second is almost the same.
    //
    // Abridged output of running BM_CacheInSteadyState benchmark from
    // raw_hash_set_benchmark.   N is the number of insert/erase operations.
    //
    //      | OLD (recover >= 7/16        | NEW (recover >= 3/32)
    // size |    N/s LoadFactor NRehashes |    N/s LoadFactor NRehashes
    //  448 | 145284       0.44        18 | 140118       0.44        19
    //  493 | 152546       0.24        11 | 151417       0.48        28
    //  538 | 151439       0.26        11 | 151152       0.53        38
    //  583 | 151765       0.28        11 | 150572       0.57        50
    //  628 | 150241       0.31        11 | 150853       0.61        66
    //  672 | 149602       0.33        12 | 150110       0.66        90
    //  717 | 149998       0.35        12 | 149531       0.70       129
    //  762 | 149836       0.37        13 | 148559       0.74       190
    //  807 | 149736       0.39        14 | 151107       0.39        14
    //  852 | 150204       0.42        15 | 151019       0.42        15
    CWISS_RawHashSet_drop_deletes_without_resize(policy, self);
  } else {
    // Otherwise grow the container.
    CWISS_RawHashSet_resize(policy, self, self->capacity_ * 2 + 1);
  }
}

static inline bool CWISS_RawHashSet_has_element(const CWISS_Policy* policy,
                                                CWISS_RawHashSet* self,
                                                const void* elem) {
  size_t hash = policy->key->hash(elem);
  CWISS_probe_seq seq = CWISS_probe(self->ctrl_, hash, self->capacity_);
  while (true) {
    CWISS_Group g = CWISS_Group_new(self->ctrl_ + seq.offset_);
    CWISS_BitMask match = CWISS_Group_Match(&g, CWISS_H2(hash));
    uint32_t i;
    while (CWISS_BitMask_next(&match, &i)) {
      char* slot =
          self->slots_ + CWISS_probe_seq_offset(&seq, i) * policy->slot->size;
      if (CWISS_LIKELY(policy->key->eq(policy->slot->get(slot), elem)))
        return true;
    }
    if (CWISS_LIKELY(CWISS_Group_MatchEmpty(&g).mask)) return false;
    CWISS_probe_seq_next(&seq);
    CWISS_DCHECK(seq.index_ <= self->capacity_, "full table!");
  }
  return false;
}

static inline void CWISS_RawHashSet_prefetch_heap_block(
    const CWISS_Policy* policy, const CWISS_RawHashSet* self) {
  // Prefetch the heap-allocated memory region to resolve potential TLB
  // misses.  This is intended to overlap with execution of calculating the
  // hash for a key.
#if defined(__GNUC__)
  __builtin_prefetch(self->ctrl_, 0, 1);
#endif  // __GNUC__
}

// Issues CPU prefetch instructions for the memory needed to find or insert
// a key.  Like all lookup functions, this support heterogeneous keys.
//
// NOTE: This is a very low level operation and should not be used without
// specific benchmarks indicating its importance.
static inline void CWISS_RawHashSet_prefetch(const CWISS_Policy* policy,
                                             const CWISS_RawHashSet* self,
                                             const void* key) {
  (void)key;
#if defined(__GNUC__)
  CWISS_RawHashSet_prefetch_heap_block(policy, self);
  CWISS_probe_seq seq =
      CWISS_probe(self->ctrl_, policy->key->hash(key), self->capacity_);
  __builtin_prefetch(self->ctrl_ + seq.offset_);
  __builtin_prefetch(self->slots_ + seq.offset_ * policy->slot->size);
#endif  // __GNUC__
}

typedef struct {
  size_t index;
  bool insert;
} CWISS_PrepareInsert;

CWISS_NOINLINE static size_t CWISS_RawHashSet_prepare_insert(
    const CWISS_Policy* policy, CWISS_RawHashSet* self, size_t hash) {
  CWISS_FindInfo target =
      CWISS_find_first_non_full(self->ctrl_, hash, self->capacity_);
  if (CWISS_UNLIKELY(self->growth_left_ == 0 &&
                     !CWISS_IsDeleted(self->ctrl_[target.offset]))) {
    CWISS_RawHashSet_rehash_and_grow_if_necessary(policy, self);
    target = CWISS_find_first_non_full(self->ctrl_, hash, self->capacity_);
  }
  ++self->size_;
  self->growth_left_ -= CWISS_IsEmpty(self->ctrl_[target.offset]);
  CWISS_SetCtrl(target.offset, CWISS_H2(hash), self->capacity_, self->ctrl_,
                self->slots_, policy->slot->size);
  // infoz().RecordInsert(hash, target.probe_length);
  return target.offset;
}

static inline CWISS_PrepareInsert CWISS_RawHashSet_find_or_prepare_insert(
    const CWISS_Policy* policy, CWISS_RawHashSet* self, const void* key) {
  CWISS_RawHashSet_prefetch_heap_block(policy, self);
  size_t hash = policy->key->hash(key);
  CWISS_probe_seq seq = CWISS_probe(self->ctrl_, hash, self->capacity_);
  while (true) {
    CWISS_Group g = CWISS_Group_new(self->ctrl_ + seq.offset_);
    CWISS_BitMask match = CWISS_Group_Match(&g, CWISS_H2(hash));
    uint32_t i;
    while (CWISS_BitMask_next(&match, &i)) {
      size_t idx = CWISS_probe_seq_offset(&seq, i);
      char* slot = self->slots_ + idx * policy->slot->size;
      if (CWISS_LIKELY(policy->key->eq(policy->slot->get(slot), key)))
        return (CWISS_PrepareInsert){idx, false};
    }
    if (CWISS_LIKELY(CWISS_Group_MatchEmpty(&g).mask)) break;
    CWISS_probe_seq_next(&seq);
    CWISS_DCHECK(seq.index_ <= self->capacity_, "full table!");
  }
  return (CWISS_PrepareInsert){
      CWISS_RawHashSet_prepare_insert(policy, self, hash), true};
}

// Constructs the value in the space pointed by the iterator. This only works
// after an unsuccessful find_or_prepare_insert() and before any other
// modifications happen in the raw_hash_set.
//
// PRECONDITION: i is an index returned from find_or_prepare_insert(k), where
// k is the key decomposed from `forward<Args>(args)...`, and the bool
// returned by find_or_prepare_insert(k) was true.
// POSTCONDITION: *m.iterator_at(i) == value_type(forward<Args>(args)...).
// TODO(#7): Provide constructor-style initialization.
static inline void* CWISS_RawHashSet_insert_at(const CWISS_Policy* policy,
                                               CWISS_RawHashSet* self, size_t i,
                                               const void* v) {
  void* dst = self->slots_ + i * policy->slot->size;
  policy->slot->init(dst);
  void* val = policy->slot->get(dst);
  policy->obj->copy(val, v);

  /* TODO
  CWISS_DCHECK(PolicyTraits::apply(FindElement{*this}, *iterator_at(i)) ==
             iterator_at(i) &&
         "constructed value does not match the lookup key");*/
  return dst;
}

static inline CWISS_RawHashSet CWISS_RawHashSet_new(const CWISS_Policy* policy,
                                                    size_t bucket_count) {
  CWISS_RawHashSet self = {
      .ctrl_ = CWISS_EmptyGroup(),
  };

  if (bucket_count) {
    self.capacity_ = CWISS_NormalizeCapacity(bucket_count);
    CWISS_RawHashSet_initialize_slots(policy, &self);
  }

  return self;
}

static inline void CWISS_RawHashSet_reserve(const CWISS_Policy* policy,
                                            CWISS_RawHashSet* self, size_t n) {
  if (n > self->size_ + self->growth_left_) {
    size_t m = CWISS_GrowthToLowerboundCapacity(n);
    CWISS_RawHashSet_resize(policy, self, CWISS_NormalizeCapacity(m));

    // This is after resize, to ensure that we have completed the allocation
    // and have potentially sampled the hashtable.
    // infoz().RecordReservation(n);
  }
}

static inline CWISS_RawHashSet CWISS_RawHashSet_dup(
    const CWISS_Policy* policy, const CWISS_RawHashSet* that) {
  CWISS_RawHashSet self = CWISS_RawHashSet_new(policy, 0);

  CWISS_RawHashSet_reserve(policy, &self, that->size_);
  // Because the table is guaranteed to be empty, we can do something faster
  // than a full `insert`.
  CWISS_RawIter iter = CWISS_RawHashSet_iter(policy, &self);
  void* v;
  while ((v = CWISS_RawIter_next(policy, &iter))) {
    size_t hash = policy->key->hash(v);

    CWISS_FindInfo target =
        CWISS_find_first_non_full(self.ctrl_, hash, self.capacity_);
    CWISS_SetCtrl(target.offset, CWISS_H2(hash), self.capacity_, self.ctrl_,
                  self.slots_, policy->slot->size);
    CWISS_RawHashSet_insert_at(policy, &self, target.offset, v);
    // infoz().RecordInsert(hash, target.probe_length);
  }
  self.size_ = that->size_;
  self.growth_left_ -= that->size_;
  return self;
}

static inline void CWISS_RawHashSet_destroy(const CWISS_Policy* policy,
                                            CWISS_RawHashSet* self) {
  CWISS_RawHashSet_destroy_slots(policy, self);
}

static inline bool CWISS_RawHashSet_empty(const CWISS_Policy* policy,
                                          const CWISS_RawHashSet* self) {
  return !self->size_;
}

static inline size_t CWISS_RawHashSet_size(const CWISS_Policy* policy,
                                           const CWISS_RawHashSet* self) {
  return self->size_;
}

static inline size_t CWISS_RawHashSet_capacity(const CWISS_Policy* policy,
                                               const CWISS_RawHashSet* self) {
  return self->capacity_;
}

static inline void CWISS_RawHashSet_clear(const CWISS_Policy* policy,
                                          CWISS_RawHashSet* self) {
  // Iterating over this container is O(bucket_count()). When bucket_count()
  // is much greater than size(), iteration becomes prohibitively expensive.
  // For clear() it is more important to reuse the allocated array when the
  // container is small because allocation takes comparatively long time
  // compared to destruction of the elements of the container. So we pick the
  // largest bucket_count() threshold for which iteration is still fast and
  // past that we simply deallocate the array.
  if (self->capacity_ > 127) {
    CWISS_RawHashSet_destroy_slots(policy, self);

    // infoz().RecordClearedReservation();
  } else if (self->capacity_) {
    if (policy->slot->del != NULL) {
      for (size_t i = 0; i != self->capacity_; ++i) {
        if (CWISS_IsFull(self->ctrl_[i])) {
          policy->slot->del(self->slots_ + i * policy->slot->size);
        }
      }
    }

    self->size_ = 0;
    CWISS_ResetCtrl(self->capacity_, self->ctrl_, self->slots_,
                    policy->slot->size);
    CWISS_RawHashSet_reset_growth_left(policy, self);
  }
  CWISS_DCHECK(!self->size_, "size was still nonzero");
  // infoz().RecordStorageChanged(0, capacity_);
}

typedef struct {
  CWISS_RawIter iter;
  bool inserted;
} CWISS_Insert;

static inline CWISS_Insert CWISS_RawHashSet_insert(const CWISS_Policy* policy,
                                                   CWISS_RawHashSet* self,
                                                   const void* val) {
  CWISS_PrepareInsert res =
      CWISS_RawHashSet_find_or_prepare_insert(policy, self, val);
  if (res.insert) {
    CWISS_RawHashSet_insert_at(policy, self, res.index, val);
  }
  return (CWISS_Insert){CWISS_RawHashSet_citer_at(policy, self, res.index),
                        res.insert};
}

static inline CWISS_RawIter CWISS_RawHashSet_find_hinted(
    const CWISS_Policy* policy, const CWISS_RawHashSet* self, const void* key,
    size_t hash) {
  CWISS_probe_seq seq = CWISS_probe(self->ctrl_, hash, self->capacity_);
  while (true) {
    CWISS_Group g = CWISS_Group_new(self->ctrl_ + seq.offset_);
    CWISS_BitMask match = CWISS_Group_Match(&g, CWISS_H2(hash));
    uint32_t i;
    while (CWISS_BitMask_next(&match, &i)) {
      char* slot =
          self->slots_ + CWISS_probe_seq_offset(&seq, i) * policy->slot->size;
      if (CWISS_LIKELY(policy->key->eq(policy->slot->get(slot), key)))
        return CWISS_RawHashSet_citer_at(policy, self,
                                         CWISS_probe_seq_offset(&seq, i));
    }
    if (CWISS_LIKELY(CWISS_Group_MatchEmpty(&g).mask))
      return (CWISS_RawIter){0};
    CWISS_probe_seq_next(&seq);
    CWISS_DCHECK(seq.index_ <= self->capacity_, "full table!");
  }
}

static inline CWISS_RawIter CWISS_RawHashSet_find(const CWISS_Policy* policy,
                                                  const CWISS_RawHashSet* self,
                                                  const void* key) {
  return CWISS_RawHashSet_find_hinted(policy, self, key,
                                      policy->key->hash(key));
}

static inline void CWISS_RawHashSet_erase_at(const CWISS_Policy* policy,
                                             CWISS_RawIter it) {
  CWISS_AssertIsFull(it.ctrl_);
  if (policy->slot->del != NULL) {
    policy->slot->del(it.slot_);
  }
  CWISS_RawHashSet_erase_meta_only(policy, it);
}

static inline size_t CWISS_RawHashSet_erase(const CWISS_Policy* policy,
                                            const CWISS_RawHashSet* self,
                                            const void* key) {
  CWISS_RawIter it = CWISS_RawHashSet_find(policy, self, key);
  if (it.slot_ == NULL) return 0;
  CWISS_RawHashSet_erase_at(policy, it);
  return 1;
}

static inline void CWISS_RawHashSet_rehash(const CWISS_Policy* policy,
                                           CWISS_RawHashSet* self, size_t n) {
  if (n == 0 && self->capacity_ == 0) return;
  if (n == 0 && self->size_ == 0) {
    CWISS_RawHashSet_destroy_slots(policy, self);
    // infoz().RecordStorageChanged(0, 0);
    // infoz().RecordClearedReservation();
    return;
  }

  // bitor is a faster way of doing `max` here. We will round up to the next
  // power-of-2-minus-1, so bitor is good enough.
  size_t m = CWISS_NormalizeCapacity(
      n | CWISS_GrowthToLowerboundCapacity(self->size_));
  // n == 0 unconditionally rehashes as per the standard.
  if (n == 0 || m > self->capacity_) {
    CWISS_RawHashSet_resize(policy, self, m);

    // This is after resize, to ensure that we have completed the allocation
    // and have potentially sampled the hashtable.
    // infoz().RecordReservation(n);
  }
}

static inline bool CWISS_RawHashSet_contains(const CWISS_Policy* policy,
                                             const CWISS_RawHashSet* self,
                                             const void* key) {
  return CWISS_RawHashSet_find(policy, self, key).slot_ != NULL;
}

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_RAW_HASH_SET_H_