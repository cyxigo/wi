<img width="150" alt="wi" src="https://github.com/user-attachments/assets/7c25acaf-bb2a-4ae7-ac22-2542f7b64676" />

## Wi is a small, fast, embeddable, prototype-based scripting language

[![GitHub release](https://img.shields.io/github/v/release/cyxigo/wi)](https://github.com/cyxigo/wi/releases/latest)
[![GitHub last commit](https://img.shields.io/github/last-commit/cyxigo/wi)](https://github.com/cyxigo/wi/commits)
[![GitHub License](https://img.shields.io/github/license/cyxigo/wi)](https://github.com/cyxigo/wi/blob/main/LICENSE)
[![Docs](https://img.shields.io/badge/docs-wiki-blue)](https://github.com/cyxigo/wi/wiki)
![Code size](https://img.shields.io/github/languages/code-size/cyxigo/wi)

```scala
obj_hunter := object {
    pounce: |self| => std::puts("${self.name} pounces on the yarn!");
};

obj_sleeper := object {
    nap: |self| => std::puts("${self.name} takes a nap in the sun.");
};

obj_cat := new obj_hunter, obj_sleeper {
    name: "Kitty";
    meow: |self| => std::puts("${self.name} says meow! (=^･ω･^=)");
};

whiskers := new obj_cat {
    name: "Whiskers";
};

whiskers->pounce(); // Whiskers pounces on the yarn!
whiskers->nap();    // Whiskers takes a nap in the sun.
whiskers->meow();   // Whiskers says meow! (=^･ω･^=)
```

- **Wi is small**. The [entire implementation](https://github.com/cyxigo/wi/tree/main/src) is under 15,000 lines of code.
- **Wi is fast**. Single-pass compilation to bytecode with compact value representation. You can check out the [benchmarks](https://github.com/cyxigo/wi/tree/main/test/benchmark#readme).
- **Wi is simple**. You can pick up the syntax and standard library in under a week from the [documentation](https://github.com/cyxigo/wi/wiki).
- **Wi is prototype-based**. No classes – just [objects](https://github.com/cyxigo/wi/wiki/Objects). You can clone or merge them to build new ones.

Want to try it? [Try it here!](https://wi-lang.pages.dev/try)

## Building

Requires [`xmake`](https://xmake.io/guide/quick-start.html#installation) and any **C99** compiler. Then simply:

```bash
xmake
```

For max speed and performance, it is better to use **GNU99** compatible compiler, since Wi uses many **GNU** extensions for performance tweaks. Wi has zero dependencies but it would be nice to have the `readline` library if you're building Wi on Linux (Wi uses it for better REPL).

This produces Wi shared library and executable.

### WASM

Requires [`emcc`](https://emscripten.org/docs/getting_started/downloads.html). Then:

```bash
xmake f -p wasm
xmake
```

This produces `wi.js` + `wi.wasm` (for functions see [`wi_wasm.c`](https://github.com/cyxigo/wi/blob/main/src/wasm/wi_wasm.c)) for embedding Wi in a web page.

You can also use [`build.sh`](https://github.com/cyxigo/wi/blob/main/util/build.sh) utility script for building Wi (Windows + Linux/WASM).

## Structure

The Wi source code is organized into the following directories:

| Directory  | Description                                                  |
| ---------- | ------------------------------------------------------------ |
| `include`  | Public API headers (`wi_conf.h`, `wi.h`, `wi.hpp`)           |
| `src/core` | Core components – compiler, VM, GC, API implementation, etc. |
| `src/std`  | Standard library implementation                              |
| `src/stm`  | Standard method library implementation                       |
| `src/wasm` | WASM embedding functions                                     |

## Notes

This programming language was created by me, and only me – a single person. It's in beta, because I'm still designing it and **breaking syntax changes can happen**, though right now – very unlikely; the project is finally gaining some stability both in syntax and in FFI/API. The standard library is designed to be minimal, but useful. Other than the FFI/API, the code is not heavily commented.

Some parts may still need polish, and I'm very open to suggestions – if you have one, [open an issue](https://github.com/cyxigo/wi/issues)!

No AI was used in the development of Wi – only me, my horrendous laptop, my favorite book (Crafting Interpreters), and tons of Googling.
