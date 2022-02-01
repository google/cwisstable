# `cwisstable.h` - SwissTables for All

`cwisstable` is a single-header C11 port of the Abseil project's
[SwissTables](https://abseil.io/about/design/swisstables).
This project is intended to bring the proven performance and flexibility
of SwissTables to C projects which cannot require a C++ compiler or for projects
which otherwise struggle with dependency management.

The public API is currently in flux, as is test coverage, but the C programs in
the `examples` directory illustrate portions of the API.

## Getting It

> TL;DR: run `./unified.py cwisstable/*.h`, grab `cwisstable.h`, and
> get cwissing.

There are two options for using this library: Bazel, or single-header.

Bazel support exists by virtue of that being how the tests and benchmarks are
build and run. All development on `cwisstable` itself requires Bazel. Downstream
projects may use `git_repository` to depend on this workspace; two build
targets are available:
- `//:split` is the "native" split form of the headers (i.e., as they are
  checked in).
- `//:unified` is the generated unified header, which is functionality
  identical.

Most C projects use other build systems: Make, CMake, Autoconf, Meson, and
artisanal shell scripts; expecting projects to fuss with Bazel is an
accessibility barrier. This is the raison d-être: you generate the unified
header, vendor it into your project, and never think about it again. Even
generating the file doesn't require installing Bazel; all you need is Python:

```sh
git clone https://github.com/google/cwisstable.git && cd cwisstable
./unify.py cwisstable/*.h
```

This will output a `cwisstable.h` file that you can vendor in; the checkout
and generation step is only necessary for upgrading the header.

That said, if you're writing C++, this library is very much not for you.
Please use https://github.com/abseil/abseil-cpp instead!

---

This is not an officially supported Google product.
