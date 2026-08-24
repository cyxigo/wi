#!/bin/python3
from pathlib import Path
from subprocess import run
from collections import defaultdict

# a little script to automate benchmarking!
# runs every test N times and picks the best time so OS noise won't get in the way
# much more reliable than me manually typing stuff
# uh um i don't know much python so this script may be a bit clunky

N = 15
LANGS = {
    ".wi": "wi",
    ".lua": "lua",
    ".rb": "ruby",
    ".py": "python3"
}

dir = Path("./test/benchmark")
scripts = [f for f in dir.iterdir() if f.is_file()]
res = defaultdict(dict)

for script in scripts:
    if not script.suffix in LANGS:
        continue

    lang = LANGS[script.suffix]
    times = []

    for _ in range(N):
        run_res = run([lang, script], capture_output=True, text=True)
        times.append(float(run_res.stdout.splitlines()[-1].removeprefix("elapsed: ")))
    
    time = min(times)
    res[script.stem.replace("_", " ").capitalize()][lang.capitalize()] = time

with open("bench.md", "w") as f:
    for test in sorted(res.keys()):
        langs = res[test]
        f.write(f"# {test}\n")
        
        sorted_langs = sorted(langs.items(), key=lambda x: float(x[1]))
        
        for lang, time in sorted_langs:
            f.write(f"- {lang}: {time:.3f}\n")
        
        f.write("\n")
