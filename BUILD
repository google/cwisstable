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

load(
    "//toolchain:copts.bzl",
    "DEFAULT_COPTS",
    "DEFAULT_LINKOPTS",
    "SAN_COPTS",
    "TEST_COPTS",
    "C_VERSION",
    "CXX_VERSION",
)

filegroup(
    name = "public_headers",
    srcs = [
        "cwisstable/declare.h",
        "cwisstable/policy.h",
        "cwisstable/hash.h",
    ],
)

filegroup(
    name = "private_headers",
    srcs = [
        "cwisstable/internal/absl_hash.h",
        "cwisstable/internal/base.h",
        "cwisstable/internal/bits.h",
        "cwisstable/internal/capacity.h",
        "cwisstable/internal/control_byte.h",
        "cwisstable/internal/extract.h",
        "cwisstable/internal/probe.h",
        "cwisstable/internal/raw_table.h",
    ],
)

cc_library(
    name = "cwisstable_split",
    hdrs = [":public_headers"],
    srcs = [":private_headers"],
    copts = DEFAULT_COPTS + C_VERSION,
    linkopts = DEFAULT_LINKOPTS,
    visibility = ["//visibility:public"],
)

genrule(
    name = "unify",
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
    name = "cwisstable",
    hdrs = ["cwisstable.h"],
    copts = DEFAULT_COPTS + C_VERSION,
    linkopts = DEFAULT_LINKOPTS,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "debug",
    hdrs = ["cwisstable/internal/debug.h"],
    srcs = ["cwisstable/internal/debug.cc"],
    copts = DEFAULT_COPTS + CXX_VERSION,
    linkopts = DEFAULT_LINKOPTS,
    deps = [":cwisstable"],
    visibility = ["//:__subpackages__"],
)

cc_library(
    name = "test_helpers",
    hdrs = ["cwisstable/internal/test_helpers.h"],
    copts = DEFAULT_COPTS + CXX_VERSION,
    linkopts = DEFAULT_LINKOPTS,
    deps = ["//:cwisstable"],
    visibility = ["//:__subpackages__"],
)

cc_test(
    name = "cwisstable_test",
    srcs = ["cwisstable/cwisstable_test.cc"],
    deps = [
        ":cwisstable",
        ":debug",
        ":test_helpers",

        "@com_google_absl//absl/cleanup",
        "@com_google_googletest//:gtest_main",
    ],
    copts = TEST_COPTS + CXX_VERSION + SAN_COPTS,
    linkopts = DEFAULT_LINKOPTS + SAN_COPTS,
)

cc_test(
    name = "cwisstable_test_nosimd",
    srcs = ["cwisstable/cwisstable_test.cc"],
    deps = [
        ":cwisstable",
        ":debug",
        ":test_helpers",

        "@com_google_absl//absl/cleanup",
        "@com_google_googletest//:gtest_main",
    ],
    defines = [
        "CWISS_HAVE_SSE2=0",
        "CWISS_HAVE_SSSE3=0",
    ],
    copts = TEST_COPTS + CXX_VERSION + SAN_COPTS,
    linkopts = DEFAULT_LINKOPTS + SAN_COPTS,
)


cc_binary(
    name = "cwisstable_benchmark",
    srcs = ["cwisstable/cwisstable_benchmark.cc"],
    tags = ["benchmark"],
    deps = [
        ":cwisstable",
        ":debug",
        ":test_helpers",
        
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/strings:str_format",
        "@com_github_google_benchmark//:benchmark_main",
    ],
    copts = TEST_COPTS + CXX_VERSION,
    linkopts = DEFAULT_LINKOPTS,
    testonly = 1,
)