These benchmarks are here to show that Wi is indeed fast in many things, and in some, of course, slow. Credits to [Wren](https://github.com/wren-lang/wren/tree/main/test/benchmark) for the benchmarks, because I suck at making and finding them.

I don't have many interpreted languages installed: Wi, Lua (5.5.1), Python (3.13.5), and Ruby (3.3.8). So we'll test these. These tests were run on my horrid laptop with an AMD Ryzen 3 7320U, 8GB of 5500 MHz DDR5 RAM. OS: Debian GNU/Linux 13 (trixie) x86_64. Languages are sorted from fastest to slowest.

# Binary trees

- Wi: 0.156
- Ruby: 0.163
- Python3: 0.172
- Lua: 0.213

# Fib

- Wi: 0.120
- Lua: 0.130
- Ruby: 0.132
- Python3: 0.208

# For

- Wi: 0.053
- Lua: 0.054
- Ruby: 0.107
- Python3: 0.110

# Method call

- Wi: 0.143
- Python3: 0.146
- Ruby: 0.151
- Lua: 0.178

As you can see, Wi **is** fast! Around Lua performance, even. While not **marginally** faster than other languages, Wi will continue to grow and mature and become even faster!  
There a lot of things to optimize and work on, Wi is still in beta you know.
