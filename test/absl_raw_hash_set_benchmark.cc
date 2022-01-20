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

// absl::raw_hash_set's benchmarks modified to run over cwisstable.

// clang-format off
#include "cwisstable.h"
#include "test/wrappers.h"
// clang-format on

#include <deque>
#include <numeric>
#include <utility>
#include <random>

#include "absl/strings/str_format.h"
#include "benchmark/benchmark.h"

namespace cwisstable::internal {
namespace {

using IntTable = raw_hash_set<int64_t, FlatSetPolicy<int64_t>>;
using StringTable = raw_hash_set<std::string, FlatSetPolicy<std::string>>;

struct string_generator {
  template <class RNG>
  std::string operator()(RNG& rng) const {
    std::string res;
    res.resize(12);
    std::uniform_int_distribution<uint32_t> printable_ascii(0x20, 0x7E);
    std::generate(res.begin(), res.end(), [&] { return printable_ascii(rng); });
    return res;
  }

  size_t size;
};

// Model a cache in steady state.
//
// On a table of size N, keep deleting the LRU entry and add a random one.
void BM_CacheInSteadyState(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  string_generator gen{12};
  StringTable t;
  std::deque<std::string> keys;
  while (t.size() < state.range(0)) {
    auto x = t.emplace(gen(rng));
    if (x.second) keys.push_back(*x.first);
  }
  CWISS_CHECK(state.range(0) >= 10, "");
  while (state.KeepRunning()) {
    // Some cache hits.
    std::deque<std::string>::const_iterator it;
    for (int i = 0; i != 90; ++i) {
      if (i % 10 == 0) it = keys.end();
      ::benchmark::DoNotOptimize(t.find(*--it));
    }
    // Some cache misses.
    for (int i = 0; i != 10; ++i) ::benchmark::DoNotOptimize(t.find(gen(rng)));
    CWISS_CHECK(t.erase(keys.front()), keys.front().c_str());
    keys.pop_front();
    while (true) {
      auto x = t.emplace(gen(rng));
      if (x.second) {
        keys.push_back(*x.first);
        break;
      }
    }
  }
  state.SetItemsProcessed(state.iterations());
  state.SetLabel(absl::StrFormat("load_factor=%.2f", t.load_factor()));
}

template <typename Benchmark>
void CacheInSteadyStateArgs(Benchmark* bm) {
  // The default.
  const float max_load_factor = 0.875;
  // When the cache is at the steady state, the probe sequence will equal
  // capacity if there is no reclamation of deleted slots. Pick a number large
  // enough to make the benchmark slow for that case.
  const size_t capacity = 1 << 10;

  // Check N data points to cover load factors in [0.4, 0.8).
  const size_t kNumPoints = 10;
  for (size_t i = 0; i != kNumPoints; ++i)
    bm->Arg(std::ceil(
        capacity * (max_load_factor + i * max_load_factor / kNumPoints) / 2));
}
BENCHMARK(BM_CacheInSteadyState)->Apply(CacheInSteadyStateArgs);

void BM_EndComparison(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  string_generator gen{12};
  StringTable t;
  while (t.size() < state.range(0)) {
    t.emplace(gen(rng));
  }

  for (auto _ : state) {
    for (auto it = t.begin(); it != t.end(); ++it) {
// This is miscompiled by GCC, so the benchmark will be messed up on that
// compiler.
#if !defined(__GNUC__) || defined(__clang__)
      benchmark::DoNotOptimize(it);
#endif
      benchmark::DoNotOptimize(t);
      benchmark::DoNotOptimize(it != t.end());
    }
  }
}
BENCHMARK(BM_EndComparison)->Arg(400);

void BM_CopyCtor(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  IntTable t;
  std::uniform_int_distribution<uint64_t> dist(0, ~uint64_t{});

  while (t.size() < state.range(0)) {
    t.emplace(dist(rng));
  }

  for (auto _ : state) {
    IntTable t2 = t;
    benchmark::DoNotOptimize(t2);
  }
}
BENCHMARK(BM_CopyCtor)->Range(128, 4096);

void BM_CopyAssign(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  IntTable t;
  std::uniform_int_distribution<uint64_t> dist(0, ~uint64_t{});
  while (t.size() < state.range(0)) {
    t.emplace(dist(rng));
  }

  IntTable t2;
  for (auto _ : state) {
    t2 = t;
    benchmark::DoNotOptimize(t2);
  }
}
BENCHMARK(BM_CopyAssign)->Range(128, 4096);

void BM_RangeCtor(benchmark::State& state) {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<uint64_t> dist(0, ~uint64_t{});
  std::vector<int> values;
  const size_t desired_size = state.range(0);
  while (values.size() < desired_size) {
    values.emplace_back(dist(rng));
  }

  for (auto unused : state) {
    IntTable t(values.begin(), values.end());
    benchmark::DoNotOptimize(t);
  }
}
BENCHMARK(BM_RangeCtor)->Range(128, 65536);

void BM_NoOpReserveIntTable(benchmark::State& state) {
  IntTable t;
  t.reserve(100000);
  for (auto _ : state) {
    benchmark::DoNotOptimize(t);
    t.reserve(100000);
  }
}
BENCHMARK(BM_NoOpReserveIntTable);

void BM_NoOpReserveStringTable(benchmark::State& state) {
  StringTable t;
  t.reserve(100000);
  for (auto _ : state) {
    benchmark::DoNotOptimize(t);
    t.reserve(100000);
  }
}
BENCHMARK(BM_NoOpReserveStringTable);

void BM_ReserveIntTable(benchmark::State& state) {
  int reserve_size = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    IntTable t;
    state.ResumeTiming();
    benchmark::DoNotOptimize(t);
    t.reserve(reserve_size);
  }
}
BENCHMARK(BM_ReserveIntTable)->Range(128, 4096);

void BM_ReserveStringTable(benchmark::State& state) {
  int reserve_size = state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
    StringTable t;
    state.ResumeTiming();
    benchmark::DoNotOptimize(t);
    t.reserve(reserve_size);
  }
}
BENCHMARK(BM_ReserveStringTable)->Range(128, 4096);

// Like std::iota, except that ctrl_t doesn't support operator++.
template <typename CtrlIter>
void Iota(CtrlIter begin, CtrlIter end, int value) {
  for (; begin != end; ++begin, ++value) {
    *begin = static_cast<ctrl_t>(value);
  }
}

void BM_Group_Match(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -4);
  Group g{group.data()};
  h2_t h = 1;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(h);
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.Match(h));
  }
}
BENCHMARK(BM_Group_Match);

void BM_Group_MatchEmpty(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -4);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.MatchEmpty());
  }
}
BENCHMARK(BM_Group_MatchEmpty);

void BM_Group_MatchEmptyOrDeleted(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -4);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.MatchEmptyOrDeleted());
  }
}
BENCHMARK(BM_Group_MatchEmptyOrDeleted);

void BM_Group_CountLeadingEmptyOrDeleted(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -2);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(g.CountLeadingEmptyOrDeleted());
  }
}
BENCHMARK(BM_Group_CountLeadingEmptyOrDeleted);

void BM_Group_MatchFirstEmptyOrDeleted(benchmark::State& state) {
  std::array<ctrl_t, Group::kWidth> group;
  Iota(group.begin(), group.end(), -2);
  Group g{group.data()};
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(g);
    ::benchmark::DoNotOptimize(*g.MatchEmptyOrDeleted());
  }
}
BENCHMARK(BM_Group_MatchFirstEmptyOrDeleted);

void BM_DropDeletes(benchmark::State& state) {
  constexpr size_t capacity = (1 << 20) - 1;
  std::vector<ctrl_t> ctrl(capacity + 1 + Group::kWidth);
  ctrl[capacity] = ctrl_t::kSentinel;
  std::vector<ctrl_t> pattern = {ctrl_t::kEmpty,   static_cast<ctrl_t>(2),
                                 ctrl_t::kDeleted, static_cast<ctrl_t>(2),
                                 ctrl_t::kEmpty,   static_cast<ctrl_t>(1),
                                 ctrl_t::kDeleted};
  for (size_t i = 0; i != capacity; ++i) {
    ctrl[i] = pattern[i % pattern.size()];
  }
  while (state.KeepRunning()) {
    state.PauseTiming();
    std::vector<ctrl_t> ctrl_copy = ctrl;
    state.ResumeTiming();
    ConvertDeletedToEmptyAndFullToDeleted(ctrl_copy.data(), capacity);
    ::benchmark::DoNotOptimize(ctrl_copy[capacity]);
  }
}
BENCHMARK(BM_DropDeletes);

}  // namespace
}  // namespace cwisstable::internal

// These methods are here to make it easy to examine the assembly for targeted
// parts of the API.
auto CodegenAbslRawHashSetInt64Find(cwisstable::internal::IntTable* table,
                                    int64_t key) -> decltype(table->find(key)) {
  return table->find(key);
}

bool CodegenAbslRawHashSetInt64FindNeEnd(cwisstable::internal::IntTable* table,
                                         int64_t key) {
  return table->find(key) != table->end();
}

auto CodegenAbslRawHashSetInt64Insert(cwisstable::internal::IntTable* table,
                                      int64_t key)
    -> decltype(table->insert(key)) {
  return table->insert(key);
}

bool CodegenAbslRawHashSetInt64Contains(cwisstable::internal::IntTable* table,
                                        int64_t key) {
  return table->contains(key);
}

void CodegenAbslRawHashSetInt64Iterate(cwisstable::internal::IntTable* table) {
  for (auto x : *table) benchmark::DoNotOptimize(x);
}

int odr =
    (::benchmark::DoNotOptimize(std::make_tuple(
         &CodegenAbslRawHashSetInt64Find, &CodegenAbslRawHashSetInt64FindNeEnd,
         &CodegenAbslRawHashSetInt64Insert, &CodegenAbslRawHashSetInt64Contains,
         &CodegenAbslRawHashSetInt64Iterate)),
     1);