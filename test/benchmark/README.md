These benchmarks are here to show that Wi is indeed fast in many things, and in some, of course, slow. Credits to [Wren](https://github.com/wren-lang/wren/tree/main/test/benchmark) for the benchmarks, because I suck at making and finding them.

I don't have many interpreted languages installed: Wi, Lua (5.4.7), Python (3.13.5), Wren (0.4.0), and Ruby (3.3.8). So we'll test these. These tests were run on my horrid laptop with an AMD Ryzen 3 7320U, 8GB of 5500 MHz DDR5 RAM. OS: Debian GNU/Linux 13 (trixie) x86_64. Languages are sorted from fastest to slowest.

## Fib

- Wi: 0.122s
- Lua : 0.149s
- Ruby: 0.173s
- Wren: 0.302s
- Python: 0.306s

## Method call

- Wi: 0.146s
- Ruby: 0.151s
- Wren: 0.159s
- Lua: 0.206s
- Python: 0.225s

## Binary trees

- Ruby: 0.177s
- Wi: 0.202s
- Wren: 0.238s
- Lua: 0.257s
- Python: 0.305s

## For

- Wi: 0.072s
- Lua: 0.085s
- Ruby: 0.103s
- Python: 0.116s
- Wren: 0.118s

As you can see, Wi **is** fast - not the **fastest**, but **fast**. There are a lot of things to optimize and work on, and Wi is in beta, sooo... Yeah... These numbers heavily depend on your OS, CPU and many many many other thingies.
