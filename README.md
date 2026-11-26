# GCC 15.2.0 ICE: `export module` crashes when a module partition creates a `std::thread` from a lambda

Minimal reproducer for an internal compiler error (segmentation fault) in GCC 15.2.0 on
x86_64-w64-mingw32. Compiling the **primary interface unit** of a module that
`export import`s a partition crashes with:

```
utility.cppm:3:8: internal compiler error: Segmentation fault
    3 | export module utility;
      |        ^~~~~~
```

The partition itself compiles fine; the crash happens when the primary interface unit is
compiled afterwards (it has to read the partition's `.gcm`).

## Environment

| | |
|---|---|
| Compiler | GCC 15.2.0, `x86_64-w64-mingw32` (MinGW-w64 build bundled with CLion 2026.3) |
| Compiler binary | `D:\jetbrains\CLion 2026.3\bin\mingw\bin\c++.exe` |
| OS | Windows 11 (zh-CN) |
| Shell | PowerShell |
| Flags | `-std=c++23 -fmodules-ts` |

Full `c++ -v` output: see [`version.txt`](version.txt).

## Reproducer

Two files, in [`minimal/`](minimal):

`minimal/utility.cppm` (primary interface unit):

```cpp
module;

export module utility;
export import :log;
```

`minimal/log.cppm` (partition `utility:log`):

```cpp
module;

#include <thread>

export module utility:log;

namespace utility {

void f()
{
    std::thread t([] {});
    t.detach();
}

}
```

## Steps

```sh
cd minimal
rm -rf gcm.cache *.o

# 1. build the partition first -- this succeeds
c++ -std=c++23 -fmodules-ts -c log.cppm -o log.o

# 2. build the primary interface unit -- this ICEs
c++ -std=c++23 -fmodules-ts -c utility.cppm -o utility.o
```

Actual output of step 2 ([`minimal/actual-output.txt`](minimal/actual-output.txt)):

```
utility.cppm:3:8: internal compiler error: Segmentation fault
    3 | export module utility;
      |        ^~~~~~
Please submit a full bug report, with preprocessed source (by using -freport-bug).
See <https://gcc.gnu.org/bugs/> for instructions.
```

Expected: both translation units compile without error.

## What triggers it (bisection results)

Starting from the real-world file that first crashed, the reproducer was shrunk by deleting
code until it stopped crashing. Findings, all with the same two-file layout:

| Partition content | Result |
|---|---|
| `void f() { std::thread t([] {}); t.detach(); }` | **ICE** |
| `void f() { std::thread t([] {}); t.join(); }` | **ICE** |
| `struct sink { sink() { std::thread t([] {}); t.join(); } };` | **ICE** |
| `struct sink { std::thread t; sink() : t([] {}) {} };` | **ICE** |
| `void f() { std::thread t(fnptr); }` (function pointer, no lambda) | ok |
| `void f() { auto l = [] {}; l(); }` (lambda only, no `std::thread`) | ok |
| `std::thread t([] {});` at namespace scope | ok |
| `void f() {}` | ok |
| empty / only `#include`s in the global module fragment | ok |

So the crash needs **both** a lambda and `std::thread` construction inside a function body of
a module partition.

## Preprocessed source

`-freport-bug` could not be used on this toolchain -- it fails before producing anything:

```
c++.exe: fatal error: cannot execute
'D:/jetbrains/CLion 2026.3/bin/mingw/bin/../libexec/gcc/x86_64-w64-mingw32/15.2.0/cc1plus.exe':
open temporary output file: No such file or directory
```

instead, `-save-temps=obj` was used, which leaves the preprocessed translation unit behind:

[`preprocessed/utility.ii`](preprocessed/utility.ii) (136 bytes -- the primary interface unit
has no includes of its own; the partition is only consumed through `gcm.cache/utility-log.gcm`).

## Origin

Found while building a CMake 4.4.2 project (`CMAKE_CXX_SCAN_FOR_MODULES ON`, Ninja generator)
that split one module into a partition. The real-world file that first reproduced it is kept
in [`original/`](original/) -- a small asynchronous logger class; the relevant part turned out
to be its constructor starting a `std::thread` from a lambda capturing `this`.

## Possibly related reports

- [PR c++/124535](https://gcc.gnu.org/pipermail/gcc-bugs/2026-March/952097.html) -- unstable
  ICE when compiling modules with partitions
- [PR c++/120013](https://gcc.gnu.org/pipermail/gcc-bugs/2025-April/912399.html) -- SIGSEGV
  ICE with modules, template instantiation across partition units
