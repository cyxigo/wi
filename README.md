<img width="152" height="107" alt="wi2" src="https://github.com/user-attachments/assets/501ff1ca-9bce-4314-832f-1ac373b9c875" /><?xml version="1.0" encoding="UTF-8" standalone="no"?>

<svg
   width="40.268044mm"
   height="28.367186mm"
   viewBox="0 0 40.268044 28.367186"
   version="1.1"
   id="svg1"
   inkscape:version="1.4.4 (dcaf3e7, 2026-05-05)"
   sodipodi:docname="wi.svg"
   inkscape:export-filename="wi.png"
   inkscape:export-xdpi="200"
   inkscape:export-ydpi="200"
   xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
   xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
  <defs
     id="defs1" />
  <g
     inkscape:label="Layer 1"
     inkscape:groupmode="layer"
     id="layer1"
     transform="translate(-84.86598,-134.31641)">
    <path
       id="path10"
       style="fill:#004fdf;stroke:#00246b;stroke-width:3.09562;stroke-dasharray:none;stroke-opacity:1;paint-order:stroke fill markers"
       d="m 88.833904,135.86422 h 32.332186 c 1.34075,0 2.42012,1.07937 2.42012,2.42012 v 20.43132 c 0,1.34075 -1.07937,2.42012 -2.42012,2.42012 H 88.833904 c -1.340741,0 -2.420112,-1.07937 -2.420112,-2.42012 v -20.43132 c 0,-1.34075 1.079371,-2.42012 2.420112,-2.42012 z"
       inkscape:export-filename="path10.png"
       inkscape:export-xdpi="600"
       inkscape:export-ydpi="600" />
    <path
       id="path9"
       style="fill:#004fdf;stroke:#5681d5;stroke-width:2.06375;stroke-dasharray:none;stroke-opacity:1;paint-order:stroke fill markers"
       d="m 88.833904,135.86422 h 32.332186 c 1.34075,0 2.42012,1.07937 2.42012,2.42012 v 20.43132 c 0,1.34075 -1.07937,2.42012 -2.42012,2.42012 H 88.833904 c -1.340741,0 -2.420112,-1.07937 -2.420112,-2.42012 v -20.43132 c 0,-1.34075 1.079371,-2.42012 2.420112,-2.42012 z"
       inkscape:export-filename="path9.png"
       inkscape:export-xdpi="600"
       inkscape:export-ydpi="600" />
    <path
       id="rect5"
       style="fill:#004fdf;stroke:#003db8;stroke-width:1.03188;stroke-dasharray:none;paint-order:stroke fill markers"
       d="m 88.833904,135.86422 h 32.332186 c 1.34075,0 2.42012,1.07937 2.42012,2.42012 v 20.43132 c 0,1.34075 -1.07937,2.42012 -2.42012,2.42012 H 88.833904 c -1.340741,0 -2.420112,-1.07937 -2.420112,-2.42012 v -20.43132 c 0,-1.34075 1.079371,-2.42012 2.420112,-2.42012 z" />
    <path
       d="m 95.452964,156.58082 -3.85852,-16.16164 h 3.34037 l 2.43638,11.10148 2.954506,-11.10148 h 3.88054 l 2.83325,11.2889 2.48047,-11.2889 h 3.28524 l -3.92465,16.16164 h -3.46164 l -3.21909,-12.08265 -3.208055,12.08265 z M 114.24939,143.2855 v -2.86632 h 3.09783 v 2.86632 z m 0,13.29532 V 144.873 h 3.09783 v 11.70782 z"
       id="text1"
       style="font-weight:bold;font-size:22.5778px;font-family:Arial;-inkscape-font-specification:'Arial Bold';fill:#ffffff;stroke-width:0.264583"
       aria-label="Wi" />
    <path
       id="path8"
       style="fill:#ffffff;fill-opacity:0.432507;fill-rule:nonzero;stroke:#003db8;stroke-width:1.59813;stroke-dasharray:none;stroke-opacity:0;paint-order:stroke fill markers"
       d="m 86.413847,145.23041 v 13.48548 c 0,1.34075 1.079266,2.42001 2.420007,2.42001 h 32.332396 c 1.34075,0 2.42,-1.07926 2.42,-2.42001 v -1.64072 A 42.773788,16.928139 0 0 0 86.413847,145.23041 Z" />
  </g>
</svg>

Wi is a small, fast, prototype-based scripting language.

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

# Why Wi?

- Wi is fast.
- Wi is simple. You can learn its syntax in less than a day.
- Wi is small. The entire implementation (compiler, VM, API) takes less than 8k lines of code.
- Wi is purely prototype-based. Many languages use classes - Wi uses objects. You clone these objects and create whatever you want.
- Wi has an FFI. You can easily extend Wi using its Foreign Function Interface and load your extensions via `load_foreign`.

# Documentation

Check out the [Wi wiki](https://github.com/cyxigo/wi/wiki) for documentation and examples.

# Building

Requires GCC and XMake to build. Then simply:

```bash
xmake
```

# Inspiration

Around 3 years ago, I was looking through interpreted programming languages (not many, of course) and realized that I didn't like any of them. Any. So then, in that very moment I decided - I will make my own simple and fast programming language. That day, the first Wi prototype was born (it wasn't even called Wi back then - it was something along the lines of "Weasel").

# Current project status

This programming language is created by me, and only me - a single person. It's in beta and I'm working on it almost every day. The standard library is not finished at all. The code is completely uncommented because... I really, really have never commented my code in my entire life. Sure, this is a terrible practice, but it's hard for me since I'm not a native English speaker. That doesn't mean I shouldn't comment my code, so... I'll try to comment it when I can. So yeah, it's a mess.

Of course, some parts of the code may contain bad practices, and I'm very open to suggestions - if you have one, open an issue!

No AI was used in the creation of this project - only me, my horrid laptop, my favorite book (Crafting Interpreters), googling, and stuff.
