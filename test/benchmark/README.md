These benchmarks are here to show that Wi is indeed fast in many things, and in some, of course, slow. Credits to [Wren](https://github.com/wren-lang/wren/tree/main/test/benchmark) for the benchmarks, because I suck at making and finding them.

I don't have many interpreted languages installed: Wi, Lua (5.5.1), Python (3.13.5), and Ruby (3.3.8). So we'll test these. These tests were run on my horrid laptop with an AMD Ryzen 3 7320U, 8GB of 5500 MHz DDR5 RAM. OS: Debian GNU/Linux 13 (trixie) x86_64. Languages are sorted from fastest to slowest.

# Binary trees

- Wi: 0.149
- Ruby: 0.159
- Python3: 0.172
- Lua: 0.188

# Fib

- Wi: 0.117
- Lua: 0.121
- Ruby: 0.131
- Python3: 0.203

# For

- Lua: 0.056
- Wi: 0.067
- Ruby: 0.106
- Python3: 0.108

# Method call

- Wi: 0.143
- Python3: 0.146
- Ruby: 0.149
- Lua: 0.169

As you can see, Wi **is** fast – not the **fastest**, but **fast**. Around Lua performance.  
There a lot of things to optimize and work on, Wi is still in beta you know.
