These benchmarks are here to show that Wi is indeed fast in many things, and in some, of course, slow. Credits to [Wren](https://github.com/wren-lang/wren/tree/main/test/benchmark) for the benchmarks, because I suck at making and finding them.

I don't have many interpreted languages installed: Wi, Lua (5.4.7), Python (3.13.5), and Ruby (3.3.8). So we'll test these. These tests were run on my horrid laptop with an AMD Ryzen 3 7320U, 8GB of 5500 MHz DDR5 RAM. OS: Debian GNU/Linux 13 (trixie) x86_64. Languages are sorted from fastest to slowest.

# Binary trees
- Ruby: 0.159
- Python3: 0.168
- Wi: 0.182
- Lua: 0.186

# Fib
- Wi: 0.120
- Ruby: 0.131
- Lua: 0.133
- Python3: 0.201

# For
- Lua: 0.043
- Wi: 0.070
- Ruby: 0.101
- Python3: 0.108

# Method call
- Wi: 0.142
- Python3: 0.146
- Ruby: 0.150
- Lua: 0.206

As you can see, Wi **is** fast – not the **fastest**, but **fast**. Around Lua performance. <br />
There a lot of things to optimize and work on, Wi is still in beta you know.
