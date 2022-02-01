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

#ifndef CWISSTABLE_PROBE_H_
#define CWISSTABLE_PROBE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cwisstable/base.h"
#include "cwisstable/bits.h"
#include "cwisstable/capacity.h"
#include "cwisstable/ctrl.h"

// Table probing functions.

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

typedef struct {
  size_t offset;
  size_t probe_length;
} CWISS_FindInfo;

// The representation of the object has two modes:
//  - small: For capacities < kWidth-1
//  - large: For the rest.
//
// Differences:
//  - In small mode we are able to use the whole capacity. The extra control
//  bytes give us at least one "empty" control byte to stop the iteration.
//  This is important to make 1 a valid capacity.
//
//  - In small mode only the first `capacity()` control bytes after the
//  sentinel are valid. The rest contain dummy ctrl_t::kEmpty values that do not
//  represent a real slot. This is important to take into account on
//  find_first_non_full(), where we never try ShouldInsertBackwards() for
//  small tables.
static inline bool CWISS_is_small(size_t capacity) {
  return capacity < CWISS_Group_kWidth - 1;
}

typedef struct {
  size_t mask_;
  size_t offset_;
  size_t index_;
} CWISS_probe_seq;

static inline CWISS_probe_seq CWISS_probe_seq_new(size_t hash, size_t mask) {
  return (CWISS_probe_seq){
      .mask_ = mask,
      .offset_ = hash & mask,
  };
}

static inline size_t CWISS_probe_seq_offset(const CWISS_probe_seq* self,
                                            size_t i) {
  return (self->offset_ + i) & self->mask_;
}

static inline void CWISS_probe_seq_next(CWISS_probe_seq* self) {
  self->index_ += CWISS_Group_kWidth;
  self->offset_ += self->index_;
  self->offset_ &= self->mask_;
}

static inline CWISS_probe_seq CWISS_probe(const CWISS_ctrl_t* ctrl, size_t hash,
                                          size_t capacity) {
  return CWISS_probe_seq_new(CWISS_H1(hash, ctrl), capacity);
}

// Probes the raw_hash_set with the probe sequence for hash and returns the
// pointer to the first empty or deleted slot.
// NOTE: this function must work with tables having both ctrl_t::kEmpty and
// ctrl_t::kDeleted in one group. Such tables appears during
// drop_deletes_without_resize.
//
// This function is very useful when insertions happen and:
// - the input is already a set
// - there are enough slots
// - the element with the hash is not in the table
static inline CWISS_FindInfo CWISS_find_first_non_full(const CWISS_ctrl_t* ctrl,
                                                       size_t hash,
                                                       size_t capacity) {
  CWISS_probe_seq seq = CWISS_probe(ctrl, hash, capacity);
  while (true) {
    CWISS_Group g = CWISS_Group_new(ctrl + seq.offset_);
    CWISS_BitMask mask = CWISS_Group_MatchEmptyOrDeleted(&g);
    if (mask.mask) {
#if !defined(NDEBUG)
      // We want to add entropy even when ASLR is not enabled.
      // In debug build we will randomly insert in either the front or back of
      // the group.
      // TODO(kfm,sbenza): revisit after we do unconditional mixing
      if (!CWISS_is_small(capacity) &&
          CWISS_ShouldInsertBackwards(hash, ctrl)) {
        return (CWISS_FindInfo){
            CWISS_probe_seq_offset(&seq, CWISS_BitMask_HighestBitSet(&mask)),
            seq.index_};
      }
#endif
      return (CWISS_FindInfo){
          CWISS_probe_seq_offset(&seq, CWISS_BitMask_TrailingZeros(&mask)),
          seq.index_};
    }
    CWISS_probe_seq_next(&seq);
    CWISS_DCHECK(seq.index_ <= capacity, "full table!");
  }
}

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_PROBE_H_