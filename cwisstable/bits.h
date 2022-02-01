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

#ifndef CWISSTABLE_BITS_H_
#define CWISSTABLE_BITS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cwisstable/base.h"

// Bit manipulation utilities.

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

// GCC-style __builtins are per-type; using ctz produces too small of a value
// on wider types, for example. This solution is polyglot, allowing it to be
// tested in C++ (vs, for example, _Generic hacks).
//
// TODO: MSCV support.
#define CWISS_TrailingZeros(x) (__builtin_ctzll((unsigned long long)(x)))
#define CWISS_LeadingZeros(x)                 \
  (__builtin_clzll((unsigned long long)(x)) - \
   (sizeof(unsigned long long) - sizeof(x)) * 8)

#define CWISS_BitWidth(x) (((uint32_t)(sizeof(x) * 8)) - CWISS_LeadingZeros(x))

typedef struct {
  uint64_t mask;
  uint32_t width, shift;
} CWISS_BitMask;

static inline uint32_t CWISS_BitMask_LowestBitSet(const CWISS_BitMask* self) {
  return CWISS_TrailingZeros(self->mask) >> self->shift;
}

static inline uint32_t CWISS_BitMask_HighestBitSet(const CWISS_BitMask* self) {
  return (uint32_t)(CWISS_BitWidth(self->mask) - 1) >> self->shift;
}

static inline uint32_t CWISS_BitMask_TrailingZeros(const CWISS_BitMask* self) {
  return CWISS_TrailingZeros(self->mask) >> self->shift;
}

static inline uint32_t CWISS_BitMask_LeadingZeros(const CWISS_BitMask* self) {
  uint32_t total_significant_bits = self->width << self->shift;
  uint32_t extra_bits = sizeof(self->mask) * 8 - total_significant_bits;
  return (uint32_t)(CWISS_LeadingZeros(self->mask << extra_bits)) >>
         self->shift;
}

// TODO: carefully document what this function is supposed to be doing.
static inline bool CWISS_BitMask_next(CWISS_BitMask* self, uint32_t* bit) {
  if (self->mask == 0) {
    return false;
  }

  *bit = CWISS_BitMask_LowestBitSet(self);
  self->mask &= (self->mask - 1);
  return true;
}

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_BITS_H_