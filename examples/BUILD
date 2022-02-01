# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cc_binary(
    name = "hashset_unified",
    srcs = ["hashset.c"],
    deps = ["//:unified"],
    copts = ["--std=c11"],
    defines = ["CWISS_EXAMPLE_UNIFIED"],
)

cc_binary(
    name = "hashset_split",
    srcs = ["hashset.c"],
    deps = ["//:split"],
    copts = ["--std=c11"],
    defines = ["CWISS_EXAMPLE_SPLIT"],
)

cc_binary(
    name = "hashmap_unified",
    srcs = ["hashmap.c"],
    deps = ["//:unified"],
    copts = ["--std=c11"],
    defines = ["CWISS_EXAMPLE_UNIFIED"],
)

cc_binary(
    name = "hashmap_split",
    srcs = ["hashmap.c"],
    deps = ["//:split"],
    copts = ["--std=c11"],
    defines = ["CWISS_EXAMPLE_SPLIT"],
)

cc_binary(
    name = "stringmap_unified",
    srcs = ["stringmap.c"],
    deps = ["//:unified"],
    copts = ["--std=c11"],
    defines = ["CWISS_EXAMPLE_UNIFIED"],
)

cc_binary(
    name = "stringmap_split",
    srcs = ["stringmap.c"],
    deps = ["//:split"],
    copts = ["--std=c11"],
    defines = ["CWISS_EXAMPLE_SPLIT"],
)
