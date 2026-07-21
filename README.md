<div align="center">
   <img width="200" alt="Wi" src="https://github.com/user-attachments/assets/08b5ac26-ad63-47e9-b654-dcaf081de906" />

   
   ## Wi is a small, fast, prototype-based scripting language.
   
   ![GitHub Tag](https://img.shields.io/github/tag-date/cyxigo/wi)
   ![GitHub License](https://img.shields.io/github/license/cyxigo/wi)
</div>

```js
var person = object {
    name = "";
    greet = function(self) {
        print("Hello, " .. self.name .. "!");
    };
};

var slava = new person {
    name = "Slava";
};

slava->greet(); // Hello, Slava!
```

- **Wi is small**. The [entire implementation](https://github.com/cyxigo/wi/tree/main/src) takes less than 8k lines of code.
- **Wi is fast**. Check out [benchmarks](https://github.com/cyxigo/wi/tree/main/test/benchmark).
- **Wi is purely prototype-based**. Many languages use classes - Wi uses [objects](https://github.com/cyxigo/wi/wiki/Objects). You clone these objects and create whatever you want.
- **Wi is simple**. You can learn its syntax in less than a week using its [documentation](https://github.com/cyxigo/wi/wiki).
- **Wi is extendable**. You can easily extend Wi using its **Foreign Function Interface** and load your extensions via [`load_foreign`](https://github.com/cyxigo/wi/wiki/FFI).

## Building

Requires `gcc` and `xmake`. Then simply:

```bash
xmake
```

Wi has zero dependencies.

## Inspiration

Around three years ago, I was looking through interpreted programming languages (not many, of course) and realized that I didn't like any of them. Any. So then, in that very moment I decided - I will make my own simple and fast programming language. That day, the first Wi prototype was born (it wasn't even called Wi back then - it was something along the lines of "Weasel").

## Current project status

This programming language is created by me, and only me - a single person. It's in beta and I'm working on it almost every day. The standard library is not finished at all. Other than the API everything is completely uncommented... I really, really have never commented my code in my entire life. Sure, this is a terrible practice, but it's hard for me since I'm not a native English speaker. That doesn't mean I shouldn't comment my code, so... I'll try to comment it when I can. So yeah, it's a mess.

Of course, some parts of the code may contain bad practices, and I'm very open to suggestions - if you have one, open an issue!

No AI was used in the creation of this project - only me, my horrid laptop, my favorite book (Crafting Interpreters), googling, and stuff.
