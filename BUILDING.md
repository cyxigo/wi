# Source code structure

The Wi source code is organized into the following directories:

| Directory  | Description                                                  |
| ---------- | ------------------------------------------------------------ |
| `include`  | Public API headers (`wi_conf.h`, `wi.h`, `wi.hpp`)           |
| `src/core` | Core components – compiler, VM, GC, API implementation, etc. |
| `src/std`  | Standard library implementation                              |
| `src/stm`  | Standard method library implementation                       |
| `src/wasm` | WASM embedding functions                                     |

# Building from source

Wi is pure **C99**, with occasional **GNU** extensions for performance tweaks.  
Wi uses [`xmake`](https://xmake.io) as its build system.

```bash
xmake
```

Although you may not always want to build Wi **binary** + **shared library** together, if so, just use `xmake build <target>` directly. Targets are:

| Name        | Description                                     |
| ----------- | ----------------------------------------------- |
| `wi`        | Executable                                      |
| `wi_shared` | Shared library (`.so`, `.dll`)                  |
| `wi_wasm`   | WASM binaries (`wi.js` glue code and `wi.wasm`) |

For WASM, you need to switch to WASM platform first via:

```bash
xmake f -p wasm
```

To switch back, just use `xmake f -p <platform>`.

Officially supported and tested compilers are **GCC**, **Clang**, **MSVC**, and **MinGW**.  
Officially supported and tested platforms are **Linux**, **Windows** and **WASM**.

WASM support is extremely minimal, only [two basic functions](https://github.com/cyxigo/wi/blob/main/src/wasm/wi_wasm.c), and no foreign libraries. Since it's only used by that [little Wi playground](https://wi-lang.pages.dev/try).

Other platforms such as macOS, BSD, etc. are not officially supported. Wi _may_ compile there, but it is not tested and foreign libraries are unavailable.

If you want to build all of the supported platforms/architectures and pack them into `.zip`s, use [`release.sh`](https://github.com/cyxigo/wi/blob/main/util/release.sh) utility script. Though, it requires A LOT of different compilers.

Wi also has two separate value representations: **NaN boxing** and **Union tagging**. The former is the main one, the latter is the fallback one in case you are restricted on providing IEEE 754 or 64-bit pointers for some reason.  
Union tagging is used if `WI_UNION_TAGGING` is defined.  
To tweak that, flip the respective flag in `xmake` config:

```bash
xmake f --union=[y or n]
```

If you want to have warnings as errors (`-Werror` on GCC, for example), `xmake` config has a flag for it too:

```bash
xmake f --werror=[y or n]
```

Both flags are opt-in.
