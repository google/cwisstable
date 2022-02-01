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

#include <algorithm>
#include <cstddef>
#include <vector>

#include "cwisstable.h"
#include "cwisstable/internal/debug.h"

namespace cwisstable::internal {

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
  enum symbol##_ForceSemi_ : int {}

// Binds a constant (macro or otherwise) from cwisstable.
#define CWISS_BIND_CONST(symbol) \
  inline static constexpr auto symbol = CWISS_##symbol

// Binds a "constructor" as an actual constructor.
#define CWISS_BIND_CTOR(type)             \
  template <typename... Ts>               \
  type(Ts... args) {                      \
    inner_ = CWISS_##type##_new(args...); \
  } /* Force a semicolon: */              \
  enum symbol##_ForceSemi_ : int {}

// Binds a "member" function; see CWISS_BIND_STRUCT
#define CWISS_BIND_MEMBER(type, symbol)                     \
  template <typename... Ts>                                 \
  auto symbol(Ts... args) {                                 \
    return CWISS_##type##_##symbol(&this->inner_, args...); \
  } /* Force a semicolon: */                                \
  enum symbol##_ForceSemi_ : int {}

// Like `CWISS_BIND_MEMBER` but wraps the return value in an aggregate.
#define CWISS_BIND_MEMBER_AS(type, symbol, as)                  \
  template <typename... Ts>                                     \
  auto symbol(Ts... args) {                                     \
    return as{CWISS_##type##_##symbol(&this->inner_, args...)}; \
  } /* Force a semicolon: */                                    \
  enum symbol##_ForceSemi_ : int {}

// Like `CWISS_BIND_MEMBER` but calls a functor on the result.
#define CWISS_BIND_MEMBER_WITH(type, symbol, with)                  \
  template <typename... Ts>                                         \
  auto symbol(Ts... args) {                                         \
    return (with)(CWISS_##type##_##symbol(&this->inner_, args...)); \
  } /* Force a semicolon: */                                        \
  enum symbol##_ForceSemi_ : int {}

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

  ctrl_t() : val_(0) {}
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
  Group(ctrl_t * p) : Group(reinterpret_cast<const CWISS_ctrl_t*>(p)) {}

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

void ConvertDeletedToEmptyAndFullToDeleted(ctrl_t* p, size_t n) {
  CWISS_ConvertDeletedToEmptyAndFullToDeleted(
      reinterpret_cast<CWISS_ctrl_t*>(p), n);
}
CWISS_BIND_FUNC(ConvertDeletedToEmptyAndFullToDeleted);
CWISS_BIND_FUNC(IsFull);

template <typename T>
struct DefaultHash {
  size_t operator()(const T& val) {
    CWISS_FxHash_State state = 0;
    CWISS_FxHash_Write(&state, &val, sizeof(T));
    return state;
  }
};

template <typename T>
struct DefaultEq {
  bool operator()(const T& a, const T& b) { return a == b; }
};

template <typename T>
struct FlatSetPolicy {
  using slot_type = T;
  using key_type = T;
  using value_type = T;

  static inline void new_slot(void* slot) {}
  static inline void del_slot(void* slot) {}
  static inline void txfer_slot(void* dst, void* src) {
    T* old = static_cast<T*>(src);
    new (dst) T(std::move(*old));
    old->~T();
  }
  static inline void* get(void* slot) { return slot; }
};

template <class InputIter>
size_t SelectBucketCountForIterRange(InputIter first, InputIter last,
                                     size_t bucket_count) {
  if (bucket_count != 0) {
    return bucket_count;
  }
  using InputIterCategory =
      typename std::iterator_traits<InputIter>::iterator_category;
  if (std::is_base_of<std::random_access_iterator_tag,
                      InputIterCategory>::value) {
    return CWISS_GrowthToLowerboundCapacity(
        static_cast<size_t>(std::distance(first, last)));
  }
  return 0;
}

// CwissTable is approximately, but not quite, raw_hash_map.
template <typename T, typename Policy, typename Hash = DefaultHash<T>,
          typename Eq = DefaultEq<T>>
class raw_hash_set {
 public:
  using key_type = typename Policy::key_type;
  using value_type = typename Policy::value_type;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using hasher = Hash;
  using key_equal = Eq;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

 private:
  static constexpr CWISS_ObjectPolicy kObjectPolicy = {
      .size = sizeof(T),
      .align = alignof(T),
      .copy =
          +[](void* dst, const void* src) {
            new (dst) T(*static_cast<const T*>(src));
          },
      .dtor = +[](void* val) { static_cast<T*>(val)->~T(); },
  };

  static constexpr CWISS_KeyPolicy kKeyPolicy = {
      .hash =
          +[](const void* val) { return Hash{}(*static_cast<const T*>(val)); },
      .eq =
          +[](const void* a, const void* b) {
            return Eq{}(*static_cast<const T*>(a), *static_cast<const T*>(b));
          },
  };
  static constexpr CWISS_AllocPolicy kAllocPolicy = {
      .alloc =
          +[](size_t size, size_t align) {
            return ::operator new(size, static_cast<std::align_val_t>(align));
          },
      .free = +[](void* ptr, size_t size,
                  size_t align) { return ::operator delete(ptr); },
  };
  static constexpr CWISS_SlotPolicy kSlotPolicy = {
      .size = sizeof(typename Policy::slot_type),
      .align = alignof(typename Policy::slot_type),
      .init = &Policy::new_slot,
      .del = &Policy::del_slot,
      .transfer = &Policy::txfer_slot,
      .get = &Policy::get,
  };
  static constexpr CWISS_Policy kPolicy = {
      .obj = &kObjectPolicy,
      .key = &kKeyPolicy,
      .alloc = &kAllocPolicy,
      .slot = &kSlotPolicy,
  };

  // Amazingly, you can stick this in class scope and it somehow
  // works. Science!
  CWISS_DECLARE_HASHSET_WITH(Inner, typename Policy::slot_type, kPolicy);

 public:
  friend class iterator;
  class iterator {
    friend class raw_hash_set;

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename raw_hash_set::value_type;
    using reference = value_type&;
    using pointer = value_type*;

    iterator() {}
    iterator(Inner_Iter it) : it_(it) {}

    reference operator*() const { return *get(); }

    pointer operator->() const { return get(); }

    // PRECONDITION: not an end() iterator.
    iterator& operator++() {
      Inner_Iter_next(&it_);
      return *this;
    }
    // PRECONDITION: not an end() iterator.
    iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    friend bool operator==(const iterator& a, const iterator& b) {
      return a.get() == b.get();
    }
    friend bool operator!=(const iterator& a, const iterator& b) {
      return !(a == b);
    }

   private:
    pointer get() const { return Inner_Iter_get(&it_); }

    Inner_Iter it_{};
  };

  friend class const_iterator;
  class const_iterator {
    friend class raw_hash_set;

   public:
    using iterator_category = typename iterator::iterator_category;
    using value_type = typename raw_hash_set::value_type;
    using reference = typename raw_hash_set::const_reference;
    using pointer = typename raw_hash_set::const_pointer;
    using difference_type = typename raw_hash_set::difference_type;

    const_iterator() {}
    // Implicit construction from iterator.
    const_iterator(iterator i) : inner_(std::move(i)) {}

    reference operator*() const { return *inner_; }
    pointer operator->() const { return inner_.operator->(); }

    const_iterator& operator++() {
      ++inner_;
      return *this;
    }
    const_iterator operator++(int) { return inner_++; }

    friend bool operator==(const const_iterator& a, const const_iterator& b) {
      return a.inner_ == b.inner_;
    }
    friend bool operator!=(const const_iterator& a, const const_iterator& b) {
      return !(a == b);
    }

   private:
    iterator inner_{};
  };

  raw_hash_set() : raw_hash_set(0) {}
  explicit raw_hash_set(size_t bucket_count)
      : inner_(Inner_new(bucket_count)) {}

  template <typename Iter>
  raw_hash_set(Iter first, Iter last, size_t bucket_count = 0)
      : raw_hash_set(SelectBucketCountForIterRange(first, last, bucket_count)) {
    insert(first, last);
  }

  template <typename U>
  raw_hash_set(std::initializer_list<U> init, size_t bucket_count = 0)
      : raw_hash_set(init.begin(), init.end(), bucket_count) {}

  raw_hash_set(const raw_hash_set& that) : inner_(Inner_dup(&that.inner_)) {}
  raw_hash_set& operator=(const raw_hash_set& that) {
    Inner_destroy(i());
    inner_ = Inner_dup(&that.inner_);
    return *this;
  }

  raw_hash_set(raw_hash_set&& that) : inner_(that.inner_) {
    that.inner_ = Inner_new(0);
  }
  raw_hash_set& operator=(raw_hash_set&& that) {
    Inner_destroy(i());
    inner_ = that.inner_;
    that.inner_ = Inner_new(0);
    return *this;
  }

  ~raw_hash_set() { Inner_destroy(i()); }

  iterator begin() { return {Inner_iter(i())}; }
  iterator end() { return {}; }

  const_iterator begin() const { return {Inner_iter(const_cast<Inner*>(i()))}; }
  const_iterator end() const { return {}; }
  const_iterator cbegin() const {
    return {Inner_iter(const_cast<Inner*>(i()))};
  }
  const_iterator cend() const { return {}; }

  bool empty() const { return Inner_empty(i()); }
  size_t size() const { return Inner_size(i()); }
  size_t capacity() const { return Inner_capacity(i()); }
  size_t max_size() const { return SIZE_MAX; }

  void clear() { return Inner_clear(i()); }

  template <typename U>
  std::pair<iterator, bool> insert(U&& v) {
    return emplace(v);
  }
  // std::pair<iterator, bool> insert(const_iterator it, const_reference v);

  template <typename Iter>
  void insert(Iter begin, Iter end) {
    for (; begin != end; ++begin) {
      emplace(*begin);
    }
  }

  template <typename U>
  void insert(std::initializer_list<U> xs) {
    insert(xs.begin(), xs.end());
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    value_type v(std::forward<Args>(args)...);
    auto val = Inner_insert(i(), &v);
    return {iterator{val.iter}, val.inserted};
  }

  template <typename... Args>
  iterator emplace_hint(const_iterator, Args&&... args) {
    return emplace(std::forward<Args>(args)...).first;
  }

  size_type erase(const key_type& v) {
    Inner_erase(i(), &v);
    return 1;
  }
  size_type erase(const_iterator it) {
    Inner_erase_at(it.inner_.it_);
    return 1;
  }
  size_type erase(iterator it) {
    Inner_erase_at(it.it_);
    return 1;
  }
  iterator erase(const_iterator begin, const_iterator end) {
    while (begin != end) {
      erase(begin++);
    }
    return end.inner_;
  }

  void rehash(size_t n) { return Inner_rehash(i(), n); }
  void reserve(size_t n) { return Inner_reserve(i(), n); }

  size_t count(const key_type& key) const { return find(key) == end() ? 0 : 1; }

  iterator find(const key_type& v, size_t hash) {
    return {Inner_find_hinted(i(), &v, hash)};
  }
  iterator find(const key_type& v) { return {Inner_find(i(), &v)}; }

  const_iterator find(const key_type& v, size_t hash) const {
    return {Inner_cfind_hinted(i(), &v, hash)};
  }
  const_iterator find(const key_type& v) const {
    return {Inner_cfind(i(), &v)};
  }

  bool contains(const key_type& v) { return Inner_contains(i(), &v); }

  size_t bucket_count() const { return capacity(); }
  float load_factor() const {
    return capacity() ? static_cast<double>(size()) / capacity() : 0.0;
  }
  float max_load_factor() const { return 1.0f; }
  void max_load_factor(float) {
    // Does nothing.
  }

  void Dump() const { Inner_dump(i()); }

  friend bool operator==(const raw_hash_set& a, const raw_hash_set& b) {
    if (a.size() != b.size()) return false;
    const raw_hash_set* outer = &a;
    const raw_hash_set* inner = &b;
    if (outer->capacity() > inner->capacity()) std::swap(outer, inner);
    for (const value_type& elem : *outer)
      if (!inner->has_element(elem)) return false;
    return true;
  }

  friend bool operator!=(const raw_hash_set& a, const raw_hash_set& b) {
    return !(a == b);
  }

  friend void swap(raw_hash_set& a, raw_hash_set& b) {
    std::swap(a.inner_, b.inner_);
  }

  friend size_t GetHashtableDebugNumProbes(const raw_hash_set& t,
                                           const key_type& key) {
    return GetHashtableDebugNumProbes(&raw_hash_set::kPolicy, &t.inner_.set_,
                                      &key);
  }

  friend std::vector<size_t> GetHashtableDebugNumProbesHistogram(
      const raw_hash_set& t) {
    return GetHashtableDebugNumProbesHistogram(&raw_hash_set::kPolicy,
                                               &t.inner_.set_);
  }

 private:
  const Inner* i() const { return &inner_; }
  Inner* i() { return &inner_; }
  Inner inner_;
};

}  // namespace cwisstable::internal
#endif  // CWISSTABLE_TEST_CC_WRAPPERS_H_
