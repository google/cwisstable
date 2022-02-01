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

#ifndef CWISSTABLE_CAPACITY_H_
#define CWISSTABLE_CAPACITY_H_

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cwisstable/base.h"
#include "cwisstable/ctrl.h"

// Functions for working with the capacity of the  backing memory of a
// SwissTable.
//
// This includes functions for calculating the putative capacity of a table and
// its interaction with how control bits are set.

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

// The number of cloned control bytes that we copy from the beginning to the
// end of the control bytes array.
#define CWISS_NumClonedBytes() ((size_t)CWISS_Group_kWidth - 1)

static inline bool CWISS_IsValidCapacity(size_t n) {
  return ((n + 1) & n) == 0 && n > 0;
}

// Returns "random" seed.
static inline size_t RandomSeed() {
#ifdef CWISS_HAVE_THREAD_LOCAL
  static thread_local size_t counter;
  size_t value = ++counter;
#else
  static volatile CWISS_ATOMIC_T(size_t) counter;
  size_t value = CWISS_ATOMIC_INC(counter);
#endif  // CWISS_HAVE_THREAD_LOCAL
  return value ^ ((size_t)&counter);
}

// Mixes a randomly generated per-process seed with `hash` and `ctrl` to
// randomize insertion order within groups.
CWISS_NOINLINE static bool CWISS_ShouldInsertBackwards(
    size_t hash, const CWISS_ctrl_t* ctrl) {
  // To avoid problems with weak hashes and single bit tests, we use % 13.
  // TODO(kfm,sbenza): revisit after we do unconditional mixing
  return (CWISS_H1(hash, ctrl) ^ RandomSeed()) % 13 > 6;
}

// PRECONDITION:
//   IsValidCapacity(capacity)
//   ctrl[capacity] == ctrl_t::kSentinel
//   ctrl[i] != ctrl_t::kSentinel for all i < capacity
// Applies mapping for every byte in ctrl:
//   DELETED -> EMPTY
//   EMPTY -> EMPTY
//   FULL -> DELETED
CWISS_NOINLINE static void CWISS_ConvertDeletedToEmptyAndFullToDeleted(
    CWISS_ctrl_t* ctrl, size_t capacity) {
  CWISS_DCHECK(ctrl[capacity] == CWISS_kSentinel, "bad ctrl value at %zu: %02x",
               capacity, ctrl[capacity]);
  CWISS_DCHECK(CWISS_IsValidCapacity(capacity), "invalid capacity: %zu",
               capacity);
  for (CWISS_ctrl_t* pos = ctrl; pos < ctrl + capacity;
       pos += CWISS_Group_kWidth) {
    CWISS_Group g = CWISS_Group_new(pos);
    CWISS_Group_ConvertSpecialToEmptyAndFullToDeleted(&g, pos);
  }
  // Copy the cloned ctrl bytes.
  memcpy(ctrl + capacity + 1, ctrl, CWISS_NumClonedBytes());
  ctrl[capacity] = CWISS_kSentinel;
}

// Reset all ctrl bytes back to ctrl_t::kEmpty, except the sentinel.
static inline void CWISS_ResetCtrl(size_t capacity, CWISS_ctrl_t* ctrl,
                                   const void* slot, size_t slot_size) {
  memset(ctrl, CWISS_kEmpty, capacity + 1 + CWISS_NumClonedBytes());
  ctrl[capacity] = CWISS_kSentinel;
  // SanitizerPoisonMemoryRegion(slot, slot_size * capacity);
}

// Sets the control byte, and if `i < NumClonedBytes()`, set the cloned byte
// at the end too.
static inline void CWISS_SetCtrl(size_t i, CWISS_ctrl_t h, size_t capacity,
                                 CWISS_ctrl_t* ctrl, const void* slot,
                                 size_t slot_size) {
  CWISS_DCHECK(i < capacity, "CWISS_SetCtrl out-of-bounds: %zu >= %zu", i,
               capacity);

  /*const char* slot_i = ((const char*) slot) + i * slot_size;
  if (CWISS_IsFull(h)) {
    SanitizerUnpoisonMemoryRegion(slot_i, slot_size);
  } else {
    SanitizerPoisonMemoryRegion(slot_i, slot_size);
  }*/

  ctrl[i] = h;
  ctrl[((i - CWISS_NumClonedBytes()) & capacity) +
       (CWISS_NumClonedBytes() & capacity)] = h;
}

// Rounds up the capacity to the next power of 2 minus 1, with a minimum of 1.
static inline size_t CWISS_NormalizeCapacity(size_t n) {
  return n ? SIZE_MAX >> CWISS_LeadingZeros(n) : 1;
}

// General notes on capacity/growth methods below:
// - We use 7/8th as maximum load factor. For 16-wide groups, that gives an
//   average of two empty slots per group.
// - For (capacity+1) >= Group::kWidth, growth is 7/8*capacity.
// - For (capacity+1) < Group::kWidth, growth == capacity. In this case, we
//   never need to probe (the whole table fits in one group) so we don't need a
//   load factor less than 1.

// Given `capacity` of the table, returns the size (i.e. number of full slots)
// at which we should grow the capacity.
static inline size_t CWISS_CapacityToGrowth(size_t capacity) {
  CWISS_DCHECK(CWISS_IsValidCapacity(capacity), "invalid capacity: %zu",
               capacity);
  // `capacity*7/8`
  if (CWISS_Group_kWidth == 8 && capacity == 7) {
    // x-x/8 does not work when x==7.
    return 6;
  }
  return capacity - capacity / 8;
}
// From desired "growth" to a lowerbound of the necessary capacity.
// Might not be a valid one and requires NormalizeCapacity().
static inline size_t CWISS_GrowthToLowerboundCapacity(size_t growth) {
  // `growth*8/7`
  if (CWISS_Group_kWidth == 8 && growth == 7) {
    // x+(x-1)/7 does not work when x==7.
    return 8;
  }
  return growth + (size_t)((((int64_t)growth) - 1) / 7);
}

// The allocated block consists of `capacity + 1 + NumClonedBytes()` control
// bytes followed by `capacity` slots, which must be aligned to `slot_align`.
// SlotOffset returns the offset of the slots into the allocated block.
static inline size_t CWISS_SlotOffset(size_t capacity, size_t slot_align) {
  CWISS_DCHECK(CWISS_IsValidCapacity(capacity), "invalid capacity: %zu",
               capacity);
  const size_t num_control_bytes = capacity + 1 + CWISS_NumClonedBytes();
  return (num_control_bytes + slot_align - 1) & (~slot_align + 1);
}

// Returns the size of the allocated block. See also above comment.
static inline size_t CWISS_AllocSize(size_t capacity, size_t slot_size,
                                     size_t slot_align) {
  return CWISS_SlotOffset(capacity, slot_align) + capacity * slot_size;
}

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_CAPACITY_H_