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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cwisstable.h"

CWISS_DECLARE_FLAT_HASHSET(MyIntSet, int);
CWISS_DECLARE_NODE_HASHMAP(MyIntMap, int, float);

static inline void kCStrPolicy_copy(void* dst, const void* src) {
  typedef struct {
    char* k;
    float v;
  } entry;
  const entry* e = (const entry*)src;
  entry* d = (entry*)dst;

  size_t len = strlen(e->k);
  d->k = malloc(len + 1);
  d->v = e->v;
  memcpy(d->k, e->k, len + 1);
}
static inline void kCStrPolicy_dtor(void* val) {
  char* str = *(char**)val;
  free(str);
}
static const CWISS_ObjectPolicy kCStrObjPolicy = {
    sizeof(struct {
      const char* c;
      float f;
    }),
    alignof(struct {
      const char* c;
      float f;
    }),
    kCStrPolicy_copy,
    kCStrPolicy_dtor,
};

static inline size_t kCStrPolicy_hash(const void* val) {
  const char* str = *(const char**)val;
  size_t len = strlen(str);
  CWISS_AbslHash_State state = CWISS_AbslHash_kInit;
  CWISS_AbslHash_Write(&state, str, len);
  return state;
}
static inline bool kCStrPolicy_eq(const void* a, const void* b) {
  const char* ap = *(const char**)a;
  const char* bp = *(const char**)b;
  return strcmp(ap, bp) == 0;
}
static const CWISS_KeyPolicy kCStrKeyPolicy = {
    kCStrPolicy_hash,
    kCStrPolicy_eq,
};

CWISS_DECLARE_NODE_SLOT_POLICY(kCStrSlotPolicy, kCStrObjPolicy, const char*);

static const CWISS_Policy kCStrPolicy = {
    &kCStrObjPolicy,
    &kCStrKeyPolicy,
    &CWISS_kDefaultAlloc,
    &kCStrSlotPolicy,
};

CWISS_DECLARE_HASHMAP_WITH(MyCStrMap, const char*, float, kCStrPolicy);

void test_set(void) {
  MyIntSet set = MyIntSet_new(8);

  for (int i = 0; i < 8; ++i) {
    int val = i * i + 1;
    MyIntSet_dump(&set);
    MyIntSet_insert(&set, &val);
  }
  MyIntSet_dump(&set);
  printf("\n");

  int k = 4;
  assert(!MyIntSet_contains(&set, &k));
  k = 5;
  MyIntSet_Iter it = MyIntSet_find(&set, &k);
  int* v = MyIntSet_Iter_get(&it);
  assert(v);
  printf("5: %p: %d\n", v, *v);

  MyIntSet_rehash(&set, 16);

  it = MyIntSet_find(&set, &k);
  v = MyIntSet_Iter_get(&it);
  assert(v);
  printf("5: %p: %d\n", v, *v);

  printf("entries:\n");
  it = MyIntSet_iter(&set);
  for (int* p = MyIntSet_Iter_get(&it); p != NULL;
       p = MyIntSet_Iter_next(&it)) {
    printf("%d\n", *p);
  }
  printf("\n");

  MyIntSet_erase(&set, &k);
  assert(!MyIntSet_contains(&set, &k));

  printf("entries:\n");
  it = MyIntSet_iter(&set);
  for (int* p = MyIntSet_Iter_get(&it); p != NULL;
       p = MyIntSet_Iter_next(&it)) {
    printf("%d\n", *p);
  }
  printf("\n");

  MyIntSet_dump(&set);
  MyIntSet_destroy(&set);
}

void test_map(void) {
  MyIntMap map = MyIntMap_new(8);

  for (int i = 0; i < 8; ++i) {
    int val = i * i + 1;
    MyIntMap_Entry e = {val, sin(val)};
    MyIntMap_dump(&map);
    MyIntMap_insert(&map, &e);
  }
  MyIntMap_dump(&map);
  printf("\n");

  int k = 4;
  assert(!MyIntMap_contains(&map, &k));
  k = 5;
  MyIntMap_Iter it = MyIntMap_find(&map, &k);
  MyIntMap_Entry* v = MyIntMap_Iter_get(&it);
  assert(v);
  printf("5: %p: %d->%f\n", v, v->key, v->val);

  MyIntMap_rehash(&map, 16);

  it = MyIntMap_find(&map, &k);
  v = MyIntMap_Iter_get(&it);
  assert(v);
  printf("5: %p: %d->%f\n", v, v->key, v->val);

  printf("entries:\n");
  it = MyIntMap_iter(&map);
  for (MyIntMap_Entry* p = MyIntMap_Iter_get(&it); p != NULL;
       p = MyIntMap_Iter_next(&it)) {
    printf("%d->%f\n", p->key, p->val);
  }
  printf("\n");

  MyIntMap_erase(&map, &k);
  assert(!MyIntMap_contains(&map, &k));

  printf("entries:\n");
  it = MyIntMap_iter(&map);
  for (MyIntMap_Entry* p = MyIntMap_Iter_get(&it); p != NULL;
       p = MyIntMap_Iter_next(&it)) {
    printf("%d->%f\n", p->key, p->val);
  }
  printf("\n");

  MyIntMap_dump(&map);
  MyIntMap_destroy(&map);
}

void test_str_map(void) {
  MyCStrMap map = MyCStrMap_new(8);

  static const char* kStrings[] = {
      "abcd", "efgh", "ijkh", "lmno", "pqrs", "tuvw", "xyza", "bcde",
  };

  for (int i = 0; i < 8; ++i) {
    int val = i * i + 1;
    MyCStrMap_Entry e = {kStrings[i], sin(val)};
    MyCStrMap_dump(&map);
    MyCStrMap_insert(&map, &e);
  }
  MyCStrMap_dump(&map);
  printf("\n");

  const char* k = "missing";
  assert(!MyCStrMap_contains(&map, &k));
  k = "lmno";
  MyCStrMap_Iter it = MyCStrMap_find(&map, &k);
  MyCStrMap_Entry* v = MyCStrMap_Iter_get(&it);
  assert(v);
  printf("5: %p: \"%s\"->%f\n", v, v->key, v->val);

  MyCStrMap_rehash(&map, 16);

  it = MyCStrMap_find(&map, &k);
  v = MyCStrMap_Iter_get(&it);
  assert(v);
  printf("5: %p: \"%s\"->%f\n", v, v->key, v->val);

  printf("entries:\n");
  it = MyCStrMap_iter(&map);
  for (MyCStrMap_Entry* p = MyCStrMap_Iter_get(&it); p != NULL;
       p = MyCStrMap_Iter_next(&it)) {
    printf("\"%s\"->%f\n", p->key, p->val);
  }
  printf("\n");

  MyCStrMap_erase(&map, &k);
  assert(!MyCStrMap_contains(&map, &k));

  printf("entries:\n");
  it = MyCStrMap_iter(&map);
  for (MyCStrMap_Entry* p = MyCStrMap_Iter_get(&it); p != NULL;
       p = MyCStrMap_Iter_next(&it)) {
    printf("\"%s\"->%f\n", p->key, p->val);
  }
  printf("\n");

  MyCStrMap_dump(&map);
  MyCStrMap_destroy(&map);
}

int main(void) {
  test_set();
  test_map();
  test_str_map();
  return 0;
}