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

#ifndef CWISSTABLE_BASE_H_
#define CWISSTABLE_BASE_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Feature detection and basic helper macros.

// C++11 compatibility macros.
#ifdef __cplusplus
  #include <atomic>
  #define CWISS_ATOMIC_T(T_) std::atomic<T_>
  #define CWISS_ATOMIC_INC(val_) (val_).fetch_add(1, std::memory_order_relaxed)

  #define CWISS_BEGIN_EXTERN_ extern "C" {
  #define CWISS_END_EXTERN_ }
#else
  #include <stdatomic.h>
  #define CWISS_ATOMIC_T(T_) _Atomic(T_)
  #define CWISS_ATOMIC_INC(val_) \
    atomic_fetch_add_explicit(&(val_), 1, memory_order_relaxed)

  #define CWISS_BEGIN_EXTERN_
  #define CWISS_END_EXTERN_
#endif

// Miscellaneous setup before entering CWISS symbol definitions. GCC likes to
// complain when you define statics and then don't use them, not realizing this
// is a header.
//
// TODO: non-GCC/Clang support?
#define CWISS_BEGIN_             \
  _Pragma("GCC diagnostic push") \
      _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
#define CWISS_END_ _Pragma("GCC diagnostic pop")

// x86 SIMD detection
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

CWISS_BEGIN_
CWISS_BEGIN_EXTERN_

// Utility macros
#define CWISS_CHECK(cond_, ...)                                           \
  do {                                                                    \
    if (cond_) break;                                                     \
    fprintf(stderr, "CWISS_CHECK failed at %s:%d\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                         \
    fprintf(stderr, "\n");                                                \
    fflush(stderr);                                                       \
    abort();                                                              \
  } while (false)

#ifdef NDEBUG
  #define CWISS_DCHECK(cond_, ...) ((void)0)
#else
  #define CWISS_DCHECK CWISS_CHECK
#endif

// Branch hot/cold annotations.
//
// TODO: Implement with appropriate intrinsics.
#define CWISS_LIKELY(cond_) cond_
#define CWISS_UNLIKELY(cond_) cond_

#define CWISS_NOINLINE __attribute__((noinline))

CWISS_END_EXTERN_
CWISS_END_

#endif  // CWISSTABLE_BASE_H_