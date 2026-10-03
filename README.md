# libdxfrw

A free, open-source C++ library for reading and writing DXF files in both ASCII and binary formats, with support for reading DWG files from R14 to 2018.

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/gpl-2.0)

## About this Fork

This is the libdxfrw used by **dxfrw_c** (a C interface and .NET binding over libdxfrw). It is based on
[codelibs/libdxfrw](https://github.com/codelibs/libdxfrw) at commit `bf80b74` (February 2026), itself a fork of the
original [libdxfrw project on SourceForge](http://sourceforge.net/projects/libdxfrw).

On top of upstream it contains several hundred fixes and additions, validated on a production set of
11,606 real drawings (9,735 DWG, 18.3 million entities): every DWG of the set is read completely.
The main ones:

- **DWG reading**: R2004/R2007 decompression fixes (no more truncated or partially read files), object-map and
  section validation, correct decoding of proxy graphics (2010+), XDATA, anonymous block names, hatch spline edges.
- **New entities**: ATTRIB/ATTDEF (block attributes) and MULTILEADER, read from DWG and DXF and written to DXF.
- **DXF writing**: hatch polyline boundaries, XDATA, extrusion/thickness, LEADER, VIEWPORT, IMAGE, compatibility
  fixes found with AutoCAD (DWG TrueView) for all output versions R12–2018.
- **Robustness**: hundreds of crashes, hangs and memory leaks on corrupt files fixed (fuzzing + AddressSanitizer).
- **Windows**: UTF-8 file paths, no iconv dependency, MinGW and MSVC builds.

The complete list, with the effect of each fix, is in [DXFRW_C_PATCHES.md](DXFRW_C_PATCHES.md) (in Romanian).
Every change in the code is marked with the comment `patch dxfrw_c`.

## Features

- **DXF Support**: Read and write DXF files (R12 to 2018) in both ASCII and binary formats
- **DWG Support**: Read DWG files from AutoCAD R14 through 2018
- **Entities**: points, lines, circles, arcs, ellipses, polylines, splines, texts, MTEXT, hatches, dimensions,
  leaders, MULTILEADER, inserts with attributes, ATTDEF, 3D faces, solids, traces, rays, xlines, images, viewports, XDATA
- **Cross-Platform**: Linux, macOS, Windows (MinGW-w64 or Visual Studio)
- **No external dependencies**
- **Open Source**: Licensed under GNU GPL v2 or later

## Table of Contents

- [Requirements](#requirements)
- [Building the Library](#building-the-library)
- [Installation](#installation)
- [Usage](#usage)
- [Tools](#tools)
- [Testing](#testing)
- [Documentation](#documentation)
- [Project Structure](#project-structure)
- [License](#license)

## Requirements

- **C++ Compiler**: C++11 compatible (GCC, Clang, MSVC, MinGW-w64)
- **CMake**: 3.13 or later

**Ubuntu/Debian:** `sudo apt-get install build-essential cmake`
**Fedora/RHEL:** `sudo dnf install gcc-c++ cmake`
**macOS:** `brew install cmake`
**Windows:** Visual Studio 2019 or later, or MinGW-w64 with CMake

## Building the Library

CMake is the only build system (the former Autotools, Visual Studio 2013 and `makefile.mingw` builds were removed:
they no longer matched the sources).

**Release binaries and compilers.** The Windows packages in the Releases page (tags `dxfrw_c-*`) are built with
MinGW-w64 gcc (MSYS2). MSVC is supported as well: the library, the tools and the 13 tests build and pass with both.
A static library only works with the compiler that built it, so `lib/libdxfrw.a` from those packages is for MinGW-w64
gcc; for Visual Studio, build the library with MSVC (`dxfrw.lib`). The two-compiler workflow (one machine with gcc,
one with MSVC) is described in `COMPILARE.md` of the `dxfrw_c` project, in Romanian.

### Linux/macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build              # optional: run the unit tests
sudo cmake --install build          # optional: install to /usr/local
```

### Windows (MinGW-w64)

With MSYS2, and `C:\msys64\mingw64\bin` and `C:\msys64\usr\bin` in `PATH`:

```bash
cmake -S . -B build -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build
cmake --install build --prefix C:/libdxfrw
```

The `"MinGW Makefiles"` generator works too when `mingw32-make` is installed.

With MinGW the tools and tests are linked statically against the C/C++ runtime (option `LIBDXFRW_STATIC_RUNTIME`).

### Windows (Visual Studio)

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64     # Visual Studio 2022: "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
cmake --install build --config Release --prefix C:/libdxfrw
```

Verified with Visual Studio Community 2026 (MSVC 14.50). The Visual Studio generator finds the compiler by itself; no
developer command prompt is needed.

Or open the folder in Visual Studio (File → Open → Folder) and build the CMake project.

### Options

| Option | Default | Meaning |
|---|---|---|
| `LIBDXFRW_BUILD_TESTS` | `ON` | Build the 13 unit tests |
| `LIBDXFRW_BUILD_TOOLS` | `ON` | Build `dwg2dxf` and `dwg2text` |
| `LIBDXFRW_STATIC_RUNTIME` | `ON` | MinGW: link the runtime statically into the executables |
| `LIBDXFRW_DEBUG_OUTPUT` | `OFF` | Keep the debug messages of `setDebug()` (reading is about twice as slow) |

### Docker Build

Portable Linux binaries for several distributions (Dockerfiles in `docker/`):

| OS | Tag | Use Case |
|----|-----|----------|
| AlmaLinux 9 | `almalinux` | RHEL/CentOS compatible |
| Ubuntu 22.04 LTS | `ubuntu` | Debian/Ubuntu compatible |
| Amazon Linux 2023 | `amazonlinux` | AWS optimized |
| Alpine Linux | `alpine` | Lightweight musl-based |

```bash
./docker/build-docker.sh run ubuntu       # creates dxfrw-ubuntu.tar.gz
./docker/build-docker.sh run all-os
sudo tar xzf dxfrw-ubuntu.tar.gz -C /opt
```

## Installation

`cmake --install` copies:

- `include/` — the public headers (`libdxfrw.h`, `drw_*.h`, `libdwgr.h`) and `include/intern/` (UTF-8 path helpers
  and the other internal headers)
- `lib/libdxfrw.a` (MinGW/GCC) or `lib/dxfrw.lib` (MSVC)
- `bin/dwg2dxf`, `bin/dwg2text`
- `share/doc/libdxfrw/` — license, README and the list of fixes

## Usage

### Reading

```cpp
#include "libdxfrw.h"
#include "drw_interface.h"

class MyInterface : public DRW_Interface {
public:
    void addLine(const DRW_Line& data) override {
        printf("Line from (%.2f, %.2f) to (%.2f, %.2f)\n",
               data.basePoint.x, data.basePoint.y, data.secPoint.x, data.secPoint.y);
    }
    void addInsert(const DRW_Insert& data) override {
        for (const DRW_Attrib& a : data.attributes)          // block attributes (ATTRIB)
            printf("%s = %s\n", a.tag.c_str(), a.text.c_str());
    }
    // ... the other pure virtual methods of DRW_Interface;
    // addAttdef() and addMLeader() are optional (empty by default)
};

int main() {
    MyInterface iface;
    dxfRW dxf("input.dxf");          // dwgR for DWG files
    if (!dxf.read(&iface, false)) {
        printf("Error reading DXF file\n");
        return 1;
    }
    return 0;
}
```

### Writing

The application implements the `write*` callbacks of `DRW_Interface` (`writeEntities()`, `writeBlocks()`, ...) and calls
the `dxfRW::write*` functions from them; `dxfRW::write(iface, DRW::AC1032, false)` writes an ASCII DXF 2018.
A complete reader/writer is in `dwg2dxf/`.

### Linking with Your Project

**CMake** (installed library):
```cmake
target_include_directories(your_target PRIVATE C:/libdxfrw/include)
target_link_libraries(your_target PRIVATE C:/libdxfrw/lib/libdxfrw.a)
```

**CMake** (as a subdirectory):
```cmake
set(LIBDXFRW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBDXFRW_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(libdxfrw)
target_link_libraries(your_target PRIVATE dxfrw)
```

**g++:** `g++ -o myapp myapp.cpp -I/usr/local/include -ldxfrw`

## Tools

| Tool | Purpose |
|---|---|
| `dwg2dxf` | Converts DWG to DXF: `dwg2dxf input.dwg [-b] [-y] -v2010 output.dxf` (versions `-R12`, `-v2000`, `-v2004`, `-v2007`, `-v2010`; run without arguments for help) |
| `dwg2text` | Extracts the texts of a DWG |
| `bin/dwg2txt` | Shell wrapper around `dwg2text` |
| `bin/dxf2txt`, `bin/dxf2txt.py` | Extracts the texts of a DXF (Python 3.9+, `pip install ezdxf`) |

## Testing

```bash
ctest --test-dir build --output-on-failure
```

13 test programs: basic, entities, polylines, text, tables, blocks, versions, errors, dimensions, annotations,
attributes, class_underflow, dwg_classes. Further test files are in the
[fess-testdata repository](https://github.com/codelibs/fess-testdata/tree/master/autocad).
dxfrw_c adds its own test suite (thousands of checks, reference files generated with ezdxf) on top of this library.

## Documentation

- **Fixes in this fork**: [DXFRW_C_PATCHES.md](DXFRW_C_PATCHES.md)
- **Specifications**: [SPECIFICATIONS.md](SPECIFICATIONS.md)
- **API documentation**: `doxygen libdxfrw.dox` (HTML in `doc/`)
- **Reference implementation**: `dwg2dxf/`

## Project Structure

```
libdxfrw/
├── src/              # Library sources and public headers
│   └── intern/       # Internal implementation (DWG/DXF readers and writers, codecs)
├── dwg2dxf/          # DWG to DXF converter
├── dwg2text/         # DWG text extractor
├── tests/            # Unit tests (CTest)
├── bin/              # Text extraction scripts
├── docker/           # Linux build images
├── build.sh          # Build script used inside the Docker images
└── CMakeLists.txt    # Build configuration
```

## License

This library is free software; you can redistribute it and/or modify it under the terms of the **GNU General Public
License** as published by the Free Software Foundation; either **version 2 of the License**, or (at your option) any
later version. See [COPYING](COPYING).

## Authors

See [AUTHORS](AUTHORS). Original author: José F. Soriano (Rallaz); fork maintained by CodeLibs; dxfrw_c fixes marked
`patch dxfrw_c` in the code.

## Changelog

See [ChangeLog](ChangeLog) for the history of the original project and [DXFRW_C_PATCHES.md](DXFRW_C_PATCHES.md) for
this fork.
