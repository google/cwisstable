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

// TODO(#3): Document everything!

#ifndef CWISSTABLE_H_
#define CWISSTABLE_H_

#include <assert.h>
#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// C++11 compatibility macros /////////////////////////////////////////////////
#ifdef __cplusplus
  #include <atomic>
  #define CWISS_ATOMIC_T(T_) std::atomic<T_>
  #define CWISS_ATOMIC_INC(val_) (val_).fetch_add(1, std::memory_order_relaxed)

  #define CWISS_BEGIN_EXTERN_ extern "C" {
  #define CWISS_END_EXTERN_ }
#else
  #include <stdatomic.h>
  #define CWISS_ATOMIC_T(T_) _Atomic T_
  #define CWISS_ATOMIC_INC(val_) \
    atomic_fetch_add_explicit(&(val_), 1, memory_order_relaxed)

  #define CWISS_BEGIN_EXTERN_
  #define CWISS_END_EXTERN_
#endif
/// C++11 compatibility macros /////////////////////////////////////////////////

/// x86 SIMD compatibility detection ///////////////////////////////////////////
#ifndef CWISS_HAVE_SSE2
  #if defined(__SSE2__) ||  \
      (defined(_MSC_VER) && \
       (defined(_M_X64) || (defined(_M_IX86) && _M_IX86_FP >= 2)))
    #define CWISS_HAVE_SSE2 1
  #else
    #define CWISS_HAVE_SSE2 0
  #endif
#endif

#ifndef CWISS_HAVE_SSSE3
  #ifdef __SSSE3__
    #define CWISS_HAVE_SSSE3 1
  #else
    #define CWISS_HAVE_SSSE3 0
  #endif
#endif

#if CWISS_HAVE_SSE2
  #include <emmintrin.h>
#endif

#if CWISS_HAVE_SSSE3
  #if !CWISS_HAVE_SSE2
    #error "Bad configuration: SSSE3 implies SSE2!"
  #endif
  #include <tmmintrin.h>
#endif
/// x86 SIMD compatibility detection ///////////////////////////////////////////

/// Utility macros /////////////////////////////////////////////////////////////
#define CWISS_CHECK(c, ...)                                               \
  do {                                                                    \
    if (c) break;                                                         \
    fprintf(stderr, "CWISS_CHECK failed at %s:%d\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                         \
    fprintf(stderr, "\n");                                                \
    fflush(stderr);                                                       \
    abort();                                                              \
  } while (false)

#ifdef NDEBUG
  #define CWISS_DCHECK(c, ...) \
    do {                       \
    } while (false)
#else
  #define CWISS_DCHECK CWISS_CHECK
#endif

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

// Branch hot/cold annotations.
//
// TODO: Implement with appropriate intrinsics.
#define CWISS_LIKELY(cond) cond
#define CWISS_UNLIKELY(cond) cond

#define CWISS_NOINLINE __attribute__((noinline))

// Miscellaneous setup before entering CWISS symbol definitions. GCC likes to
// complain when you define statics and then don't use them, not realizing this
// is a header.
//
// TODO: non-GCC/Clang support?
#define CWISS_BEGIN_             \
  _Pragma("GCC diagnostic push") \
      _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
#define CWISS_END_ _Pragma("GCC diagnostic pop")
/// Utility macros /////////////////////////////////////////////////////////////

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

/// CWISS_BitMask //////////////////////////////////////////////////////////////
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
/// CWISS_BitMask //////////////////////////////////////////////////////////////

/// CWISS_ctrl /////////////////////////////////////////////////////////////////
// The values here are selected for maximum performance. See the static
// CWISS_DCHECKs below for details.
typedef int8_t CWISS_ctrl_t;
#define CWISS_kEmpty (INT8_C(-128))
#define CWISS_kDeleted (INT8_C(-2))
#define CWISS_kSentinel (INT8_C(-1))
// TODO: Wrap CWISS_ctrl_t in a single-field struct to get strict-aliasing
// benefits.

static_assert(
    (CWISS_kEmpty & CWISS_kDeleted & CWISS_kSentinel & 0x80) != 0,
    "Special markers need to have the MSB to make checking for them efficient");
static_assert(
    CWISS_kEmpty < CWISS_kSentinel && CWISS_kDeleted < CWISS_kSentinel,
    "CWISS_kEmpty and CWISS_kDeleted must be smaller than "
    "CWISS_kSentinel to make the SIMD test of IsEmptyOrDeleted() efficient");
static_assert(
    CWISS_kSentinel == -1,
    "CWISS_kSentinel must be -1 to elide loading it from memory into SIMD "
    "registers (pcmpeqd xmm, xmm)");
static_assert(CWISS_kEmpty == -128,
              "CWISS_kEmpty must be -128 to make the SIMD check for its "
              "existence efficient (psignb xmm, xmm)");
static_assert(
    (~CWISS_kEmpty & ~CWISS_kDeleted & CWISS_kSentinel & 0x7F) != 0,
    "CWISS_kEmpty and CWISS_kDeleted must share an unset bit that is not "
    "shared by CWISS_kSentinel to make the scalar test for "
    "MatchEmptyOrDeleted() efficient");
static_assert(CWISS_kDeleted == -2,
              "CWISS_kDeleted must be -2 to make the implementation of "
              "ConvertSpecialToEmptyAndFullToDeleted efficient");

// A single block of empty control bytes for tables without any slots allocated.
// This enables removing a branch in the hot path of find().
alignas(16) static const CWISS_ctrl_t CWISS_kEmptyGroup[16] = {
    CWISS_kSentinel, CWISS_kEmpty, CWISS_kEmpty, CWISS_kEmpty,
    CWISS_kEmpty,    CWISS_kEmpty, CWISS_kEmpty, CWISS_kEmpty,
    CWISS_kEmpty,    CWISS_kEmpty, CWISS_kEmpty, CWISS_kEmpty,
    CWISS_kEmpty,    CWISS_kEmpty, CWISS_kEmpty, CWISS_kEmpty};
static inline CWISS_ctrl_t* CWISS_EmptyGroup() {
  return (CWISS_ctrl_t*)&CWISS_kEmptyGroup;
}

// Returns a hash seed.
//
// The seed consists of the ctrl_ pointer, which adds enough entropy to ensure
// non-determinism of iteration order in most cases.
static inline size_t CWISS_HashSeed(const CWISS_ctrl_t* ctrl) {
  // The low bits of the pointer have little or no entropy because of
  // alignment. We shift the pointer to try to use higher entropy bits. A
  // good number seems to be 12 bits, because that aligns with page size.
  return ((uintptr_t)ctrl) >> 12;
}

static inline size_t CWISS_H1(size_t hash, const CWISS_ctrl_t* ctrl) {
  return (hash >> 7) ^ CWISS_HashSeed(ctrl);
}

typedef uint8_t CWISS_h2_t;
static inline CWISS_h2_t CWISS_H2(size_t hash) { return hash & 0x7F; }

static inline bool CWISS_IsEmpty(CWISS_ctrl_t c) { return c == CWISS_kEmpty; }
static inline bool CWISS_IsFull(CWISS_ctrl_t c) { return c >= 0; }
static inline bool CWISS_IsDeleted(CWISS_ctrl_t c) {
  return c == CWISS_kDeleted;
}
static inline bool CWISS_IsEmptyOrDeleted(CWISS_ctrl_t c) {
  return c < CWISS_kSentinel;
}
/// CWISS_ctrl /////////////////////////////////////////////////////////////////

/// CWISS_Group ////////////////////////////////////////////////////////////////
#define CWISS_Group_BitMask(x) \
  (CWISS_BitMask){(uint64_t)(x), CWISS_Group_kWidth, CWISS_Group_kShift};

// TODO(#4): Port this to NEON.
#if CWISS_HAVE_SSE2
// https://github.com/abseil/abseil-cpp/issues/209
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87853
// _mm_cmpgt_epi8 is broken under GCC with -funsigned-char
// Work around this by using the portable implementation of Group
// when using -funsigned-char under GCC.
static inline __m128i CWISS_mm_cmpgt_epi8_fixed(__m128i a, __m128i b) {
  #if defined(__GNUC__) && !defined(__clang__)
  if (CHAR_MIN == 0) {  // std::is_unsigned_v<char>
    const __m128i mask = _mm_set1_epi8(0x80);
    const __m128i diff = _mm_subs_epi8(b, a);
    return _mm_cmpeq_epi8(_mm_and_si128(diff, mask), mask);
  }
  #endif
  return _mm_cmpgt_epi8(a, b);
}

typedef __m128i CWISS_Group;
  #define CWISS_Group_kWidth ((size_t)16)
  #define CWISS_Group_kShift 0

CWISS_Group CWISS_Group_new(const CWISS_ctrl_t* pos) {
  return _mm_loadu_si128((const __m128i*)pos);
}

// Returns a bitmask representing the positions of slots that match hash.
static inline CWISS_BitMask CWISS_Group_Match(const CWISS_Group* self,
                                              CWISS_h2_t hash) {
  return CWISS_Group_BitMask(
      _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_set1_epi8(hash), *self)))
}

// Returns a bitmask representing the positions of empty slots.
static inline CWISS_BitMask CWISS_Group_MatchEmpty(const CWISS_Group* self) {
  #if CWISS_HAVE_SSSE3
  // This only works because ctrl_t::kEmpty is -128.
  return CWISS_Group_BitMask(_mm_movemask_epi8(_mm_sign_epi8(*self, *self))
  #else
  return CWISS_Group_Match(self, CWISS_kEmpty);
  #endif
}

// Returns a bitmask representing the positions of empty or deleted slots.
static inline CWISS_BitMask CWISS_Group_MatchEmptyOrDeleted(
    const CWISS_Group* self) {
  __m128i special = _mm_set1_epi8((uint8_t)CWISS_kSentinel);
  return CWISS_Group_BitMask(
      _mm_movemask_epi8(CWISS_mm_cmpgt_epi8_fixed(special, *self)));
}

// Returns the number of trailing empty or deleted elements in the group.
static inline uint32_t CWISS_Group_CountLeadingEmptyOrDeleted(
    const CWISS_Group* self) {
  __m128i special = _mm_set1_epi8((uint8_t)CWISS_kSentinel);
  return CWISS_TrailingZeros((uint32_t)(
      _mm_movemask_epi8(CWISS_mm_cmpgt_epi8_fixed(special, *self)) + 1));
}

static inline void CWISS_Group_ConvertSpecialToEmptyAndFullToDeleted(
    const CWISS_Group* self, CWISS_ctrl_t* dst) {
  __m128i msbs = _mm_set1_epi8((char)-128);
  __m128i x126 = _mm_set1_epi8(126);
  #if CWISS_HAVE_SSSE3
  __m128i res = _mm_or_si128(_mm_shuffle_epi8(x126, *self), msbs);
  #else
  __m128i zero = _mm_setzero_si128();
  __m128i special_mask = CWISS_mm_cmpgt_epi8_fixed(zero, *self);
  __m128i res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
  #endif
  _mm_storeu_si128((__m128i*)dst, res);
}
#else  // CWISS_HAVE_SSE2
typedef uint64_t CWISS_Group;
  #define CWISS_Group_kWidth ((size_t)8)
  #define CWISS_Group_kShift 3

// NOTE: Endian-hostile.
CWISS_Group CWISS_Group_new(const CWISS_ctrl_t* pos) {
  CWISS_Group val;
  memcpy(&val, pos, sizeof(val));
  return val;
}

static inline CWISS_BitMask CWISS_Group_Match(const CWISS_Group* self,
                                              CWISS_h2_t hash) {
  // For the technique, see:
  // http://graphics.stanford.edu/~seander/bithacks.html##ValueInWord
  // (Determine if a word has a byte equal to n).
  //
  // Caveat: there are false positives but:
  // - they only occur if there is a real match
  // - they never occur on ctrl_t::kEmpty, ctrl_t::kDeleted, ctrl_t::kSentinel
  // - they will be handled gracefully by subsequent checks in code
  //
  // Example:
  //   v = 0x1716151413121110
  //   hash = 0x12
  //   retval = (v - lsbs) & ~v & msbs = 0x0000000080800000
  uint64_t msbs = 0x8080808080808080ULL;
  uint64_t lsbs = 0x0101010101010101ULL;
  uint64_t x = *self ^ (lsbs * hash);
  return CWISS_Group_BitMask((x - lsbs) & ~x & msbs);
}

static inline CWISS_BitMask CWISS_Group_MatchEmpty(const CWISS_Group* self) {
  uint64_t msbs = 0x8080808080808080ULL;
  return CWISS_Group_BitMask((*self & (~*self << 6)) & msbs);
}

static inline CWISS_BitMask CWISS_Group_MatchEmptyOrDeleted(
    const CWISS_Group* self) {
  uint64_t msbs = 0x8080808080808080ULL;
  return CWISS_Group_BitMask((*self & (~*self << 7)) & msbs);
}

static inline uint32_t CWISS_Group_CountLeadingEmptyOrDeleted(
    const CWISS_Group* self) {
  uint64_t gaps = 0x00FEFEFEFEFEFEFEULL;
  return (CWISS_TrailingZeros(((~*self & (*self >> 7)) | gaps) + 1) + 7) >> 3;
}

static inline void CWISS_Group_ConvertSpecialToEmptyAndFullToDeleted(
    const CWISS_Group* self, CWISS_ctrl_t* dst) {
  uint64_t msbs = 0x8080808080808080ULL;
  uint64_t lsbs = 0x0101010101010101ULL;
  uint64_t x = *self & msbs;
  uint64_t res = (~x + (x >> 7)) & ~lsbs;
  memcpy(dst, &res, sizeof(*dst));
}
#endif  // CWISS_HAVE_SSE2
/// CWISS_Group ////////////////////////////////////////////////////////////////

/// Capacity/insertion formulae ////////////////////////////////////////////////
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
#endif  // ABSL_HAVE_THREAD_LOCAL
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

// template <class InputIter>
// size_t SelectBucketCountForIterRange(InputIter first, InputIter last,
//                                      size_t bucket_count) {
//   if (bucket_count != 0) {
//     return bucket_count;
//   }
//   using InputIterCategory =
//       typename std::iterator_traits<InputIter>::iterator_category;
//   if (std::is_base_of<std::random_access_iterator_tag,
//                       InputIterCategory>::value) {
//     return GrowthToLowerboundCapacity(
//         static_cast<size_t>(std::distance(first, last)));
//   }
//   return 0;
// }

#define CWISS_AssertIsFull(ctrl)                                               \
  CWISS_CHECK((ctrl) != NULL && CWISS_IsFull(*(ctrl)),                         \
              "Invalid operation on iterator (%p/%d). The element might have " \
              "been erased, or the table might have rehashed.",                \
              (ctrl), (ctrl) ? *(ctrl) : -1)

#define CWISS_AssertIsValid(ctrl)                                              \
  CWISS_CHECK((ctrl) == NULL || CWISS_IsFull(*(ctrl)),                         \
              "Invalid operation on iterator (%p/%d). The element might have " \
              "been erased, or the table might have rehashed.",                \
              (ctrl), (ctrl) ? *(ctrl) : -1)
/// Capacity/insertion formulae ////////////////////////////////////////////////

/// Probing ////////////////////////////////////////////////////////////////////
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
/// Probing ////////////////////////////////////////////////////////////////////

/// ctrl_t operations //////////////////////////////////////////////////////////
// TODO: Fold int ctrl_t section?

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
/// ctrl_t operations //////////////////////////////////////////////////////////

/// Allocation formulae ////////////////////////////////////////////////////////
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

static inline void* CWISS_malloc(size_t size, size_t align) {
  void* p = malloc(size);  // TODO: Check alignment.
  CWISS_CHECK(p != NULL, "malloc() returned null");
  return p;
}
static inline void CWISS_free(void* array, size_t size, size_t align) {
  free(array);
}
/// Allocation formulae ////////////////////////////////////////////////////////

/// Hash functions /////////////////////////////////////////////////////////////
typedef size_t CWISS_FxHash_State;
#define CWISS_FxHash_kInit ((CWISS_FxHash_State)0);
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
/// Hash functions /////////////////////////////////////////////////////////////

/// Policies ///////////////////////////////////////////////////////////////////
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
    CWISS_FxHash_State state = CWISS_FxHash_kInit;                                  \
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

static const CWISS_AllocPolicy CWISS_kDefaultAlloc = {CWISS_malloc, CWISS_free};

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
    void* node = CWISS_malloc(kObject_.size, kObject_.align);              \
    memcpy(slot, &node, sizeof(node));                                     \
  }                                                                        \
  static inline void kPolicy_##_slot_del(void* slot) {                     \
    if (kObject_.dtor != NULL) {                                           \
      kObject_.dtor(*((void**)slot));                                      \
    }                                                                      \
    CWISS_free(*(void**)slot, kObject_.size, kObject_.align);              \
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

/// Policies ///////////////////////////////////////////////////////////////////

/// RawHashSet /////////////////////////////////////////////////////////////////
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
  CWISS_RawIter iter = CWISS_RawHashSet_citer(policy, that);
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
/// RawHashSet /////////////////////////////////////////////////////////////////

CWISS_END_
CWISS_END_EXTERN_

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

#endif  // CWISSTABLE_H_
