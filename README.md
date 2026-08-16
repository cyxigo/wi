<div align="center">
   <img width="200" alt="Wi" src="https://github.com/user-attachments/assets/08b5ac26-ad63-47e9-b654-dcaf081de906" />

## Wi is a small, fast, prototype-based scripting language.

[![GitHub release](https://img.shields.io/github/v/release/cyxigo/wi)](https://github.com/cyxigo/wi/releases/latest)
[![GitHub last commit](https://img.shields.io/github/last-commit/cyxigo/wi)](https://github.com/cyxigo/wi/commits)
[![GitHub License](https://img.shields.io/github/license/cyxigo/wi)](https://github.com/cyxigo/wi/blob/main/LICENSE)
[![Docs](https://img.shields.io/badge/docs-wiki-blue)](https://github.com/cyxigo/wi/wiki)
![Code size](https://img.shields.io/github/languages/code-size/cyxigo/wi)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey)
[![CodeQL Advanced](https://github.com/cyxigo/wi/actions/workflows/codeql.yml/badge.svg)](https://github.com/cyxigo/wi/actions/workflows/codeql.yml)

</div>

```js
var obj_person = object {
    name = "";
    greet = function(self) {
        print("Hello, " .. self.name .. "!");
    };
};

var slava = new obj_person {
    name = "Slava";
};

slava->greet(); // Hello, Slava!
```

- **Wi is small**. The [entire implementation](https://github.com/cyxigo/wi/tree/main/src) takes less than 10k lines of code.
- **Wi is fast**. Check out [benchmarks](https://github.com/cyxigo/wi/tree/main/test/benchmark).
- **Wi is purely prototype-based**. Many languages use classes - Wi uses [objects](https://github.com/cyxigo/wi/wiki/Objects). You clone these objects and create whatever you want.
- **Wi is simple**. You can learn its syntax in less than a week using its [documentation](https://github.com/cyxigo/wi/wiki).
- **Wi is extendable**. You can easily extend Wi using its **Foreign Function Interface** and load your extensions via [`load`](https://github.com/cyxigo/wi/wiki/FFI) statement.

You can even [try Wi online!](https://cyxigo.github.io/wi/)

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
This produces `wi.js` + `wi.wasm`, with functions `wi_wasm_init`/`wi_wasm_run`/`wi_wasm_get_error` (see [`wi_wasm.c`](https://github.com/cyxigo/wi/blob/main/src/wasm/wi_wasm.c)) for embedding Wi in a web page.


You can also use [`build.sh`](https://github.com/cyxigo/wi/blob/main/build.sh)/[`build_wasm.sh`](https://github.com/cyxigo/wi/blob/main/build_wasm.sh) utility scripts for building Wi.

## Inspiration

Around three years ago, I was looking through interpreted programming languages (not many, of course) and realized that I didn't like any of them. Any. So then, in that very moment I decided - I will make my own simple and fast programming language. That day, the first Wi prototype was born (it wasn't even called Wi back then - it was something along the lines of "Weasel").

## Current project status

This programming language was created by me, and only me - a single person. It's in beta and I'm working on it almost every day. The standard library isn't finished yet, but it's already quite useful. Other than the API, the code is not heavily commented, only the parts that can be really confusing (WIP).

Some parts may still need polish, and I'm very open to suggestions - if you have one, open an issue!

No AI was used in the creation of this project - only me, my horrid laptop, my favorite book (Crafting Interpreters), Googling, and stuff.
