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

filegroup(
    name = "public_headers",
    srcs = [
        "cwisstable/declare.h",
        "cwisstable/policy.h",
    ],
)

filegroup(
    name = "private_headers",
    srcs = [
        "cwisstable/internal/base.h",
        "cwisstable/internal/bits.h",
        "cwisstable/internal/capacity.h",
        "cwisstable/internal/ctrl.h",
        "cwisstable/internal/extract.h",
        "cwisstable/internal/probe.h",
        "cwisstable/internal/raw_hash_set.h",
    ],
)

cc_library(
  name = "split",
  hdrs = [":public_headers"],
  srcs = [":private_headers"],
  visibility = ["//visibility:public"],
)

genrule(
    name = "generate_unified",
    srcs = [
        ":public_headers",
        ":private_headers",
    ],
    outs = ["cwisstable.h"],
    cmd = '''
        ./$(location unify.py) \
            --out "$@" \
            --include_dir=$$(dirname $(location unify.py)) \
            $(locations :public_headers)
    ''',
    tools = ["unify.py"],
    message = "Generating unified cwisstable.h",
)

cc_library(
  name = "unified",
  hdrs = ["cwisstable.h"],
  visibility = ["//visibility:public"],
)

cc_library(
    name = "debug",
    hdrs = ["cwisstable/internal/debug.h"],
    srcs = ["cwisstable/internal/debug.cc"],
    deps = [":unified"],
    visibility = ["//:__subpackages__"],
)