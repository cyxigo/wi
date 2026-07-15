These benchmarks are here to show that Wi is indeed fast in many things, and in some, of course, slow. Credits to [Wren](https://github.com/wren-lang/wren/tree/main/test/benchmark) for the benchmarks, because I suck at making and finding them.

I don't have many interpreted languages installed: Wi, Lua (5.5.0), Python (3.12.12), Wren (0.4.0), and Ruby (4.0.6). So we'll test these. These tests were run on my horrid laptop with an AMD Ryzen 3 7320U, 8GB of 5500 MHz DDR5 RAM. Languages are sorted from fastest to slowest.

## Fib

- Wi: 0.145s
- Ruby: 0.147s
- Lua : 0.150s
- Python: 0.265s
- Wren: 0.268s

## Method call

- Wren: 0.140s
- Ruby: 0.144s
- Python: 0.156s
- Wi: 0.178s
- Lua: 0.200s

## Binary trees

- Ruby: 0.200s
- Wren: 0.370s
- Lua: 0.400s
- Wi: 0.436s
- Python: 0.500s

## For

- Lua: 0.042s
- Wi: 0.100s
- Wren: 0.100s
- Ruby: 0.196s
- Python: 0.203s

As you can see, Wi **is** fast - not the **fastest**, but **fast**. There are a lot of things to optimize and work on, and Wi is in beta, sooo... Yeah.
