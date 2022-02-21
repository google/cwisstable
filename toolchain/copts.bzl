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

# C/C++ compiler options selection. Taken from Abseil.

CLANG_CL_FLAGS = [
    "/W3",
    "/DNOMINMAX",
    "/DWIN32_LEAN_AND_MEAN",
    "/D_CRT_SECURE_NO_WARNINGS",
    "/D_SCL_SECURE_NO_WARNINGS",
    "/D_ENABLE_EXTENDED_ALIGNED_STORAGE",
]

CLANG_CL_TEST_FLAGS = [
    "-Wno-c99-extensions",
    "-Wno-deprecated-declarations",
    "-Wno-missing-noreturn",
    "-Wno-missing-prototypes",
    "-Wno-missing-variable-declarations",
    "-Wno-null-conversion",
    "-Wno-shadow",
    "-Wno-shift-sign-overflow",
    "-Wno-sign-compare",
    "-Wno-unused-function",
    "-Wno-unused-member-function",
    "-Wno-unused-parameter",
    "-Wno-unused-private-field",
    "-Wno-unused-template",
    "-Wno-used-but-marked-unused",
    "-Wno-zero-as-null-pointer-constant",
    "-Wno-gnu-zero-variadic-macro-arguments",
]

GCC_FLAGS = [
    "-Werror",
    "-Wall",
    "-Wextra",
    "-Wcast-qual",
    # The cc1 driver whines about this in C mode.
    # "-Wconversion-null",
    "-Wformat-security",
    "-Wmissing-declarations",
    "-Woverlength-strings",
    "-Wpointer-arith",
    "-Wundef",
    "-Wunused-local-typedefs",
    "-Wunused-result",
    "-Wvarargs",
    "-Wvla",
    "-Wwrite-strings",
    "-DNOMINMAX",
]

GCC_TEST_FLAGS = [
    "-Wno-conversion-null",
    "-Wno-deprecated-declarations",
    "-Wno-missing-declarations",
    "-Wno-sign-compare",
    "-Wno-unused-function",
    "-Wno-unused-parameter",
    "-Wno-unused-private-field",
]

LLVM_FLAGS = [
    "-Werror",
    "-Wall",
    "-Wextra",
    "-Wcast-qual",
    "-Wconversion",
    "-Wfloat-overflow-conversion",
    "-Wfloat-zero-conversion",
    "-Wfor-loop-analysis",
    "-Wformat-security",
    "-Wgnu-redeclared-enum",
    "-Winfinite-recursion",
    "-Winvalid-constexpr",
    "-Wliteral-conversion",
    "-Wmissing-declarations",
    "-Woverlength-strings",
    "-Wpointer-arith",
    "-Wself-assign",
    "-Wshadow-all",
    "-Wstring-conversion",
    "-Wtautological-overlap-compare",
    "-Wundef",
    "-Wuninitialized",
    "-Wunreachable-code",
    "-Wunused-comparison",
    "-Wunused-local-typedefs",
    "-Wunused-result",
    "-Wvla",
    "-Wwrite-strings",
    "-Wno-float-conversion",
    "-Wno-implicit-float-conversion",
    "-Wno-implicit-int-float-conversion",
    "-Wno-implicit-int-conversion",
    "-Wno-shorten-64-to-32",
    "-Wno-sign-conversion",
    "-Wno-unknown-warning-option",
    "-DNOMINMAX",
]

LLVM_TEST_FLAGS = [
    "-Wno-c99-extensions",
    "-Wno-deprecated-declarations",
    "-Wno-missing-noreturn",
    "-Wno-missing-prototypes",
    "-Wno-missing-variable-declarations",
    "-Wno-null-conversion",
    "-Wno-shadow",
    "-Wno-shift-sign-overflow",
    "-Wno-sign-compare",
    "-Wno-unused-function",
    "-Wno-unused-member-function",
    "-Wno-unused-parameter",
    "-Wno-unused-private-field",
    "-Wno-unused-template",
    "-Wno-used-but-marked-unused",
    "-Wno-zero-as-null-pointer-constant",
    "-Wno-gnu-zero-variadic-macro-arguments",
    # Make sure we get all the SIMD features on tests with Clang.
    "-march=native",
]

MSVC_FLAGS = [
    "/W3",
    "/DNOMINMAX",
    "/DWIN32_LEAN_AND_MEAN",
    "/D_CRT_SECURE_NO_WARNINGS",
    "/D_SCL_SECURE_NO_WARNINGS",
    "/D_ENABLE_EXTENDED_ALIGNED_STORAGE",
    "/bigobj",
    "/wd4005",
    "/wd4068",
    "/wd4180",
    "/wd4244",
    "/wd4267",
    "/wd4503",
    "/wd4800",
]

MSVC_LINKOPTS = [
    "-ignore:4221",
]

MSVC_TEST_FLAGS = [
    "/wd4018",
    "/wd4101",
    "/wd4503",
    "/wd4996",
    "/DNOMINMAX",
]

LLVM_SANTIZER_FLAGS = [
    "-fsanitize=address",
]

DEFAULT_COPTS = select({
    "//toolchain:is_msvc": MSVC_FLAGS,
    "//toolchain:is_clang_cl": CLANG_CL_FLAGS,
    "//toolchain:is_clang": LLVM_FLAGS,
    "//conditions:default": GCC_FLAGS,
})

TEST_COPTS = DEFAULT_COPTS + select({
    "//toolchain:is_msvc": MSVC_TEST_FLAGS,
    "//toolchain:is_clang_cl": CLANG_CL_TEST_FLAGS,
    "//toolchain:is_clang": LLVM_TEST_FLAGS,
    "//conditions:default": GCC_TEST_FLAGS,
})

SAN_COPTS = select({
    "//toolchain:is_clang": LLVM_SANTIZER_FLAGS,
    "//conditions:default": [],
})

DEFAULT_LINKOPTS = select({
    "//toolchain:is_msvc": MSVC_LINKOPTS,
    "//conditions:default": [],
})

CXX_VERSION = select({
    "//toolchain:is_msvc": ["/std:c++17"],
    "//toolchain:is_clang_cl": ["/std:c++17"],
    "//conditions:default": ["--std=c++17"],
})

C_VERSION = select({
    "//toolchain:is_msvc": ["/std:c11"],
    "//toolchain:is_clang_cl": ["/std:c11"],
    "//conditions:default": ["--std=c11"],
})