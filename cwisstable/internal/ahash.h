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

#ifndef CWISSTABLE_INTERNAL_AES_HASH_H_
#define CWISSTABLE_INTERNAL_AES_HASH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cwisstable/internal/base.h"
#include "cwisstable/internal/bits.h"

/// Implementation details of aHash.
///
/// Based on the Apache-2.0-licensed code found at
/// https://github.com/tkaitchuck/aHash/blob/master/src/aes_hash.rs

CWISS_BEGIN
CWISS_BEGIN_EXTERN

#if CWISS_HAVE_AES

typedef struct {
  CWISS_U128 enc_, sum_, key_;
} CWISS_AHash_State_;

  // This is a keyed hash, so it requires "random" inputs. However, because
  // its cryptographic power is unproven, we use constants for the initial value
  // to avoid the overhead of randomness. You are welcome to inject randomness
  // into your build system by defining these constants via
  // -DCWISS_AHash_kInitN_.
  //
  // These numbers are the first eight SHA-256 round constants.

  #if !defined(CWISS_AHash_kInit0_) && !defined(CWISS_AHash_kInit1_) && \
      !defined(CWISS_AHash_kInit2_) && !defined(CWISS_AHash_kInit3_)
    #define CWISS_AHash_kInit0_ ((uint64_t)0x71374491428a2f98)
    #define CWISS_AHash_kInit1_ ((uint64_t)0xe9b5dba5b5c0fbcf)
    #define CWISS_AHash_kInit2_ ((uint64_t)0x59f111f13956c25b)
    #define CWISS_AHash_kInit3_ ((uint64_t)0xab1c5ed5923f82a4)
  #endif

  #define CWISS_AHash_kInit_                           \
    ((CWISS_AHash_State_){                             \
        {CWISS_AHash_kInit0_, CWISS_AHash_kInit1_},    \
        {CWISS_AHash_kInit2_, CWISS_AHash_kInit3_},    \
        {                                              \
            CWISS_AHash_kInit0_ ^ CWISS_AHash_kInit2_, \
            CWISS_AHash_kInit1_ ^ CWISS_AHash_kInit3_, \
        },                                             \
    })

CWISS_INLINE_ALWAYS
static inline CWISS_U128 CWISS_AHash_AddLanes(CWISS_U128 a, CWISS_U128 b) {
  __m128i a_, b_;
  memcpy(&a_, &a, sizeof(a));
  memcpy(&b_, &b, sizeof(b));

  a_ = _mm_add_epi64(a_, b_);

  memcpy(&a, &a_, sizeof(a));
  return a;
}

CWISS_INLINE_ALWAYS
static inline CWISS_U128 CWISS_AHash_ShuffleAndAdd(CWISS_U128 a, CWISS_U128 b) {
  #if CWISS_HAVE_SSSE3
  const uint64_t mask[2] = {0x050f0d0806090b04, 0x020a07000c01030e};

  __m128i a_, b_, mask_;
  memcpy(&a_, &a, sizeof(a));
  memcpy(&b_, &b, sizeof(b));
  memcpy(&mask_, &mask, sizeof(mask));

  a_ = _mm_shuffle_epi8(a_, mask_);
  a_ = _mm_add_epi64(a_, b_);

  memcpy(&a, &a_, sizeof(a));
  return a;
  #else
  // bswap of u128.
  char* a_bytes = (char*)&a;
  for (size_t i = 0; i < sizeof(a) / 2; ++i) {
    a_bytes[i] = a_bytes[sizeof(a) - i - 1];
  }

  return CWISS_AHash_AddLanes(a, b);
  #endif
}

CWISS_INLINE_ALWAYS
static inline void CWISS_AHash_Mix1(CWISS_AHash_State_* self, CWISS_U128 v1) {
  self->enc_ = CWISS_AesEnc(self->enc_, v1);
  self->sum_ = CWISS_AHash_ShuffleAndAdd(self->sum_, v1);
}

CWISS_INLINE_ALWAYS
static inline void CWISS_AHash_Mix2(CWISS_AHash_State_* self, CWISS_U128 v1,
                                    CWISS_U128 v2) {
  self->enc_ = CWISS_AesEnc(self->enc_, v1);
  self->sum_ = CWISS_AHash_ShuffleAndAdd(self->sum_, v1);
  self->enc_ = CWISS_AesEnc(self->enc_, v2);
  self->sum_ = CWISS_AHash_ShuffleAndAdd(self->sum_, v2);
}

static inline void CWISS_AHash_Write_(CWISS_AHash_State_* self, const void* val,
                                      size_t len) {
  const char* val8 = (const char*)val;
  self->enc_.lo += len;

  if (len > 64) {
    CWISS_U128 tail[4];
    memcpy(tail, val8 + len - sizeof(tail), sizeof(tail));

    CWISS_U128 current[4] = {
    CWISS_AesEnc(self->key_, tail[0]),
    CWISS_AesEnc(self->key_, tail[1]),
    CWISS_AesEnc(self->key_, tail[2]),
    CWISS_AesEnc(self->key_, tail[3]),
  };

    CWISS_U128 sum[2] = {
        CWISS_AHash_AddLanes(self->key_, tail[0]),
        CWISS_AHash_AddLanes(self->key_, tail[1]),
    };
    sum[0] = CWISS_AHash_ShuffleAndAdd(sum[0], tail[2]);
    sum[1] = CWISS_AHash_ShuffleAndAdd(sum[1], tail[3]);

    while (len > 64) {
      CWISS_U128 blocks[4];
      memcpy(blocks, val8, sizeof(tail));
      val8 += sizeof(tail);

      current[0] = CWISS_AesEnc(current[0], blocks[0]);
      current[1] = CWISS_AesEnc(current[1], blocks[1]);
      current[2] = CWISS_AesEnc(current[2], blocks[2]);
      current[3] = CWISS_AesEnc(current[3], blocks[3]);
      sum[0] = CWISS_AHash_ShuffleAndAdd(sum[0], blocks[0]);
      sum[1] = CWISS_AHash_ShuffleAndAdd(sum[1], blocks[1]);
      sum[0] = CWISS_AHash_ShuffleAndAdd(sum[0], blocks[2]);
      sum[1] = CWISS_AHash_ShuffleAndAdd(sum[1], blocks[3]);
    }

    CWISS_AHash_Mix2(self, CWISS_AesEnc(current[0], current[1]),
                     CWISS_AesEnc(current[2], current[3]));
    CWISS_AHash_Mix1(self, CWISS_AHash_AddLanes(sum[0], sum[1]));
  } else if (len > 32) {
    // Len 33..=64.
    CWISS_U128 head[2];
    CWISS_U128 tail[2];
    memcpy(head, val8, sizeof(head));
    memcpy(tail, val8 + len - sizeof(tail), sizeof(tail));
    CWISS_AHash_Mix2(self, head[0], head[1]);
    CWISS_AHash_Mix2(self, tail[0], tail[1]);
  } else if (len > 16) {
    // Len 17..=32.
    CWISS_U128 head, tail;
    memcpy(&head, val8, sizeof(head));
    memcpy(&tail, val8 + len - sizeof(tail), sizeof(tail));
    CWISS_AHash_Mix2(self, head, tail);

  } else if (len > 8) {
    // Len 9..=16.
    uint64_t head, tail;
    memcpy(&head, val8, sizeof(head));
    memcpy(&tail, val8 + len - sizeof(tail), sizeof(tail));
    CWISS_AHash_Mix1(self, (CWISS_U128){head, tail});
  } else {
    CWISS_AHash_Mix1(self, CWISS_Load0to8Twice(val, len));
  }
}

static inline uint64_t CWISS_AHash_Finish_(CWISS_AHash_State_ self) {
  CWISS_U128 combined = CWISS_AesDec(self.sum_, self.enc_);
  return CWISS_AesEnc(CWISS_AesEnc(combined, self.key_), combined).lo;
}

#endif  // CWISS_HAVE_AES

CWISS_END_EXTERN
CWISS_END

#endif  // CWISSTABLE_INTERNAL_AES_HASH_H_