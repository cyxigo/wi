<div align="center">
   <img width="200" alt="wi" src="https://github.com/user-attachments/assets/7c25acaf-bb2a-4ae7-ac22-2542f7b64676" />

## Wi is a small, fast, embeddable, prototype-based scripting language

[![GitHub release](https://img.shields.io/github/v/release/cyxigo/wi)](https://github.com/cyxigo/wi/releases/latest)
[![GitHub last commit](https://img.shields.io/github/last-commit/cyxigo/wi)](https://github.com/cyxigo/wi/commits)
[![GitHub License](https://img.shields.io/github/license/cyxigo/wi)](https://github.com/cyxigo/wi/blob/main/LICENSE)
[![Docs](https://img.shields.io/badge/docs-wiki-blue)](https://github.com/cyxigo/wi/wiki)
![Code size](https://img.shields.io/github/languages/code-size/cyxigo/wi)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey)
[![CodeQL Advanced](https://github.com/cyxigo/wi/actions/workflows/codeql.yml/badge.svg)](https://github.com/cyxigo/wi/actions/workflows/codeql.yml)

</div>

```scala
obj_person := object {
    name: "";
    greet: |self| => print("Hello, ${self.name}!");
};

slava := new obj_person {
    name: "Slava";
};

slava->greet(); // Hello, Slava!
```

- **Wi is small**. The [entire implementation](https://github.com/cyxigo/wi/tree/main/src) takes less than 15,000 lines of code.
- **Wi is fast**. Fast single-pass compiler to bytecode with **NaN** boxing value representation help Wi to [compete with other dynamic programming languages](https://github.com/cyxigo/wi/tree/main/test/benchmark#readme).
- **Wi is simple**. You can learn its dead-simple syntax and standard library in less than a week using its [lovingly made documentation](https://github.com/cyxigo/wi/wiki).
- **Wi is prototype-based**. Many languages use classes – Wi uses [objects](https://github.com/cyxigo/wi/wiki/Objects). You clone these objects and create whatever you want. No delegation, just cloning.
- **Wi is extendable**. [Simple and straightforward FFI](https://github.com/cyxigo/wi/wiki/Wi-API-Reference) allows you to easily create libraries for Wi and use them with the ease of [one statement](https://github.com/cyxigo/wi/wiki/FFI).

Want to try it? [Try it here!](https://cyxigo.github.io/wi/)

## Building

Requires [`xmake`](https://xmake.io/guide/quick-start.html#installation) and any **C99** compiler. Then simply:

```bash
xmake
```

For max speed and performance, it is better to use **GNU99** compatible compiler, since Wi uses many **GNU** extensions for performance tweaks. Wi has zero dependencies but it would be nice to have `readline` library if you're building Wi on Linux (Wi uses it for better REPL).

This produces Wi shared library + Wi executable.
### WASM
Requires [`emcc`](https://emscripten.org/docs/getting_started/downloads.html). Then:
```bash
xmake f -p wasm
xmake
```
This produces `wi.js` + `wi.wasm` (for functions see [`wi_wasm.c`](https://github.com/cyxigo/wi/blob/main/src/wasm/wi_wasm.c)) for embedding Wi in a web page.

You can also use [`build.sh`](https://github.com/cyxigo/wi/blob/main/build.sh)/[`build_wasm.sh`](https://github.com/cyxigo/wi/blob/main/build_wasm.sh) utility scripts for building Wi.

## Structure
The Wi source code is organized into the following directories:

| Directory  | Description                                                  |
| ---------- | ------------------------------------------------------------ |
| `include`  | Public API headers (`wi_conf.h`, `wi.h`, `wi.hpp`)           |
| `src/core` | Core components – compiler, VM, GC, API implementation, ect. |
| `src/std`  | Standard library implementation                              |
| `src/stm`  | Standard method library implementation                       |
| `src/wasm` | WASM embedding functions                                     |

## Status

This programming language was created by me, and only me – a single person. It's in beta, because I'm still designing it and **breaking syntax changes are to be expected**. The standard library isn't finished yet, but it's already quite useful. Other than the API, the code is not heavily commented.

Some parts may still need polish, and I'm very open to suggestions – if you have one, [open an issue](https://github.com/cyxigo/wi/issues)!

No AI was used in the development of Wi – only me, my horrendous laptop, my favorite book (Crafting Interpreters), and tons of Googling.
