# `cwisstable.h`

`cwisstable` is a single-header C11 port of the Abseil project's
[Swiss Tables](https://abseil.io/about/design/swisstables).
This project is intended to bring the proven performance and flexibility
of Swiss Tables to C projects which cannot require a C++ compiler or for projects
which otherwise struggle with dependency management.

The public API is currently in flux, as is test coverage, but `example.c` shows off
how the library can be used to create a new hash set type and insert values into it. 

---

This is not an officially supported Google product.
