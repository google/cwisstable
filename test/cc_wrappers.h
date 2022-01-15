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

// C++ wrappers with an absl::raw_hash_set API for binding to the tests.

#ifndef CWISSTABLE_TEST_CC_WRAPPERS_H_
#define CWISSTABLE_TEST_CC_WRAPPERS_H_

#include <cstddef>
#include <vector>

#include "cwisstable.h"
#include "cwisstable_debug.h"

namespace cwisstable {

template <typename T>
struct BindingWrapper {
  T inner_;
};

// Binds a struct.
#define CWISS_BIND_STRUCT(type) struct type : BindingWrapper<CWISS_##type>

// Binds a type alias.
#define CWISS_BIND_TYPE(type) using type = CWISS_##type;

// Binds a function from cwisstable.
#define CWISS_BIND_FUNC(symbol)     \
  template <typename... Ts>         \
  auto symbol(Ts... args) {         \
    return CWISS_##symbol(args...); \
  } /* Force a semicolon: */        \
  enum : int {}

// Binds a constant (macro or otherwise) from cwisstable.
#define CWISS_BIND_CONST(symbol) \
  inline static constexpr auto symbol = CWISS_##symbol

// Binds a "constructor" as an actual constructor.
#define CWISS_BIND_CTOR(type)             \
  template <typename... Ts>               \
  type(Ts... args) {                      \
    inner_ = CWISS_##type##_new(args...); \
  } /* Force a semicolon: */              \
  enum : int {}

// Binds a "member" function; see CWISS_BIND_STRUCT
#define CWISS_BIND_MEMBER(type, symbol)                     \
  template <typename... Ts>                                 \
  auto symbol(Ts... args) {                                 \
    return CWISS_##type##_##symbol(&this->inner_, args...); \
  } /* Force a semicolon: */                                \
  enum : int {}

// Like `CWISS_BIND_MEMBER` but wraps the return value in an aggregate.
#define CWISS_BIND_MEMBER_AS(type, symbol, as)                  \
  template <typename... Ts>                                     \
  auto symbol(Ts... args) {                                     \
    return as{CWISS_##type##_##symbol(&this->inner_, args...)}; \
  } /* Force a semicolon: */                                    \
  enum : int {}

// Like `CWISS_BIND_MEMBER` but calls a functor on the result.
#define CWISS_BIND_MEMBER_WITH(type, symbol, with)                  \
  template <typename... Ts>                                         \
  auto symbol(Ts... args) {                                         \
    return (with)(CWISS_##type##_##symbol(&this->inner_, args...)); \
  } /* Force a semicolon: */                                        \
  enum : int {}

// Binds a "member" constant.
#define CWISS_BIND_MEMBER_CONST(type, symbol) \
  inline static constexpr auto symbol = CWISS_##type##_##symbol

CWISS_BIND_STRUCT(probe_seq) {
  CWISS_BIND_CTOR(probe_seq);
  CWISS_BIND_MEMBER(probe_seq, offset);
  CWISS_BIND_MEMBER(probe_seq, next);

  size_t offset() { return inner_.offset_; }

  size_t index() { return inner_.index_; }
};

// This is slightly sketchy; none of the tests do anything interesting with
// the type thankfully.
template <typename Ignored, int width, int shift = 0>
CWISS_BIND_STRUCT(BitMask) {
  BitMask(uint64_t mask) { inner_ = {mask, width, shift}; }

  CWISS_BIND_MEMBER(BitMask, LowestBitSet);
  CWISS_BIND_MEMBER(BitMask, HighestBitSet);
  CWISS_BIND_MEMBER(BitMask, TrailingZeros);
  CWISS_BIND_MEMBER(BitMask, LeadingZeros);
  CWISS_BIND_MEMBER_AS(BitMask, next, bool);

  // These are useful for unit tests (gunit).
  using value_type = int;
  using iterator = BitMask;
  using const_iterator = BitMask;

  BitMask& operator++() {
    uint32_t ignored;
    next(&ignored);
    return *this;
  }
  explicit operator bool() const { return inner_.mask != 0; }
  uint32_t operator*() const {
    BitMask copy = *this;
    uint32_t val;
    copy.next(&val);
    return val;
  }

  BitMask begin() const { return *this; }
  BitMask end() const { return BitMask(0); }

  friend bool operator==(const BitMask& a, const BitMask& b) {
    return a.inner_.mask == b.inner_.mask;
  }
  friend bool operator!=(const BitMask& a, const BitMask& b) {
    return a.inner_.mask != b.inner_.mask;
  }
};

// Emulate an enum class. It... kind of works?
struct ctrl_t {
  CWISS_BIND_CONST(kDeleted);
  CWISS_BIND_CONST(kEmpty);
  CWISS_BIND_CONST(kSentinel);

  ctrl_t(CWISS_ctrl_t val) : val_(val) {}
  operator CWISS_ctrl_t() { return val_; }
  CWISS_ctrl_t val_;
};

CWISS_BIND_TYPE(h2_t);

CWISS_BIND_FUNC(NormalizeCapacity);
CWISS_BIND_FUNC(GrowthToLowerboundCapacity);
CWISS_BIND_FUNC(CapacityToGrowth);

CWISS_BIND_STRUCT(Group) {
  // This is actually fine because CWISS_ctrl_t is going to be a character type
  // on ~every platform.
  Group(const ctrl_t* p) : Group(reinterpret_cast<const CWISS_ctrl_t*>(p)) {}

  CWISS_BIND_CTOR(Group);
  CWISS_BIND_MEMBER_CONST(Group, kWidth);
  CWISS_BIND_MEMBER_CONST(Group, kShift);
  CWISS_BIND_MEMBER_WITH(Group, Match, ([](auto x) {
                           BitMask<void, kWidth, kShift> y(0);
                           y.inner_ = x;
                           return y;
                         }));

  CWISS_BIND_MEMBER_WITH(Group, MatchEmpty, ([](auto x) {
                           BitMask<void, kWidth, kShift> y(0);
                           y.inner_ = x;
                           return y;
                         }));

  CWISS_BIND_MEMBER_WITH(Group, MatchEmptyOrDeleted, ([](auto x) {
                           BitMask<void, kWidth, kShift> y(0);
                           y.inner_ = x;
                           return y;
                         }));

  CWISS_BIND_MEMBER(Group, CountLeadingEmptyOrDeleted);
  CWISS_BIND_MEMBER(Group, ConvertDeletedToEmptyAndFullToDeleted);
};
CWISS_BIND_FUNC(EmptyGroup);

CWISS_BIND_FUNC(ConvertDeletedToEmptyAndFullToDeleted);
CWISS_BIND_FUNC(IsFull);

// CwissTable is approximately, but not quite, raw_hash_map.
template <typename V, CWISS_Hasher Hash = CWISS_Policy_HashDefault>
struct UnprefixTable {
  static constexpr CWISS_Policy kPolicy = {
      .size = sizeof(V),
      .align = alignof(V),
      .hash = +[](const void* val, size_t len) { return Hash(val, len); },
      .eq =
          +[](const void* a, const void* b, size_t) {
            return *static_cast<const V*>(a) == *static_cast<const V*>(b);
          },
      .alloc =
          +[](size_t size, size_t align) {
            return ::operator new(size, static_cast<std::align_val_t>(align));
          },
      .free =
          +[](void* ptr, size_t size, size_t align) {
            return ::operator delete(ptr, size,
                                     static_cast<std::align_val_t>(align));
          },
      .destroy = +[](void* x, size_t) { static_cast<V*>(x)->~V(); },
      .copy = +[](void* dst, const void* src,
                  size_t) { new (dst) V(*static_cast<const V*>(src)); },
      .move =
          +[](void* dst, const void* src, size_t) {
            V* old = const_cast<V*>(static_cast<const V*>(src));
            new (dst) V(std::move(*old));
          },
  };

  // Amazingly, you can stick this in class scope and it somehow
  // works. Science!
  CWISS_DECLARE_FLAT_HASHSET_WITH(Inner, V, kPolicy);

  struct iter {
    V& operator*() const { return *operator->(); }
    V* operator->() const { return Inner_Iter_get(&it_); }
    iter operator++() {
      Inner_Iter_next(&it_);
      return *this;
    }
    iter operator++(int) {
      iter prev = *this;
      ++*this;
      return prev;
    }
    friend bool operator==(const iter& a, const iter& b) {
      return Inner_Iter_get(&a.it_) == Inner_Iter_get(&b.it_);
    }
    friend bool operator!=(const iter& a, const iter& b) { return !(a == b); }

    Inner_Iter it_;
  };
  iter begin() { return {Inner_iter(i())}; }
  iter begin() const { return {Inner_iter(const_cast<Inner*>(i()))}; }
  iter end() const { return {}; }

  using value_type = V;
  using iterator = iter;
  using const_iterator = iter;

  UnprefixTable() : inner_(Inner_new(0)) {}
  UnprefixTable(size_t n) : inner_(Inner_new(n)) {}

  size_t capacity() const { return Inner_capacity(i()); }
  size_t bucket_count() const { return Inner_capacity(i()); }
  size_t size() const { return Inner_size(i()); }
  bool empty() const { return Inner_empty(i()); }

  void reserve(size_t n) { return Inner_reserve(i(), n); }
  void clear() { return Inner_clear(i()); }

  iter find(const V& v) { return {Inner_find(i(), &v)}; }
  bool contains(const V& v) { return Inner_contains(i(), &v); }

  template <typename... Args>
  std::pair<iter, bool> emplace(Args&&... args) {
    V v(std::forward<Args>(args)...);
    auto val = Inner_insert(i(), &v);
    return {iter{val.iter}, val.inserted};
  }
  std::pair<iter, bool> insert(const V& v) { return emplace(v); }
  template <typename I1, typename I2>
  void insert(I1 begin, I2 end) {
    for (; begin != end; ++begin) {
      emplace(*begin);
    }
  }

  size_t erase(const V& v) {
    Inner_erase(i(), &v);
    return 1;
  }
  size_t erase(iter it) {
    Inner_erase_at(it.it_);
    return 1;
  }
  void erase(iter begin, iter end) {
    while (begin != end) {
      erase(begin++);
    }
  }

  friend size_t GetHashtableDebugNumProbes(const UnprefixTable& t,
                                           const V& key) {
    return GetHashtableDebugNumProbes(&UnprefixTable::kPolicy, &t.inner_.set_,
                                      &key);
  }

  friend std::vector<size_t> GetHashtableDebugNumProbesHistogram(
      const UnprefixTable& t) {
    return GetHashtableDebugNumProbesHistogram(&UnprefixTable::kPolicy,
                                               &t.inner_.set_);
  }

  const Inner* i() const { return &inner_; }
  Inner* i() { return &inner_; }
  Inner inner_;
};

}  // namespace cwisstable
#endif  // CWISSTABLE_TEST_CC_WRAPPERS_H_
