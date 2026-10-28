<a href="https://wi-lang.pages.dev">
    <img width="150" alt="wi" src="https://github.com/user-attachments/assets/7c25acaf-bb2a-4ae7-ac22-2542f7b64676" />
</a>

## Wi is a small, fast, embeddable, prototype-based scripting language

[![GitHub release](https://img.shields.io/github/v/release/cyxigo/wi)](https://github.com/cyxigo/wi/releases/latest)
[![GitHub last commit](https://img.shields.io/github/last-commit/cyxigo/wi)](https://github.com/cyxigo/wi/commits)
[![GitHub License](https://img.shields.io/github/license/cyxigo/wi)](https://github.com/cyxigo/wi/blob/main/LICENSE)
[![Docs](https://img.shields.io/badge/docs-wiki-blue)](https://github.com/cyxigo/wi/wiki)
![Code size](https://img.shields.io/github/languages/code-size/cyxigo/wi)

```
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

whiskers->pounce(); %% Whiskers pounces on the yarn!
whiskers->nap();    %% Whiskers takes a nap in the sun.
whiskers->meow();   %% Whiskers says meow! (=^･ω･^=)
```

- **Wi is small**. The [entire implementation](https://github.com/cyxigo/wi/tree/main/src) is under 15,000 lines of code.
- **Wi is fast**. Single-pass compilation to bytecode with compact value representation. You can check out the [benchmarks](https://github.com/cyxigo/wi/tree/main/test/benchmark#readme).
- **Wi is simple**. You can pick up the syntax and standard library in under a week from the [documentation](https://github.com/cyxigo/wi/wiki).
- **Wi is prototype-based**. No classes – just [objects](https://github.com/cyxigo/wi/wiki/Objects). You can clone or merge them to build new ones.

Want to try it? [Try it here!](https://wi-lang.pages.dev/try)

## Building from source

For the source code structure and build instructions, please refer to [`BUILDING.md`](https://github.com/cyxigo/wi/blob/main/BUILDING.md).

## Notes

### Supported platforms

Officially supported and tested platforms are **Linux**, **Windows** and **WASM**.

Other platforms such as macOS, BSD, etc. are not officially supported. Wi _may_ compile there, but it is not tested and foreign libraries are unavailable.

### What `beta` actually means for Wi

It means **breaking syntax and API/FFI changes can happen**. But don't be scared right away! Wi v9 seriously improved on that. Right now, syntax changes are highly unlikely, and FFI/API changes only happen for a good reason (like making DX better). If anything does break, it's documented right away in the [CHANGELOG.md](https://github.com/cyxigo/wi/blob/main/CHANGELOG.md) and the wiki is updated right after.

### Thread safety

If you're embedding Wi, and dealing with multiple threads, just follow one simple rule: one state = one thread. Kind of like Lua does it.

### Use of AI

Wi is written by hand. For better, or worse.

Some parts may still need polish, and I'm very open to suggestions – if you have one, [open an issue](https://github.com/cyxigo/wi/issues)!
