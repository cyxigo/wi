#!/bin/python3
from pathlib import Path
from subprocess import run

# runs every test/bug and test/lang test and compares its output to the respective .wiwi file
# first line of every .wi script must be either $ok (compare stdout) or $fail (compare stderr)
# might be pretty silly but works nonetheless
#
# why .wiwi? because .out is git-ignored and idk i couldn't come up with something better
wi = "wi"
failed = 0


def test(path, recursive=False):
    path = Path(path)
    scripts = path.rglob("*.wi") if recursive else path.glob("*.wi")

    for script in sorted(scripts):
        out = script.with_suffix(".wiwi")

        if not out.is_file():
            print(f"[SKIP] {script} has no respective .wiwi file")
            continue

        kind = script.read_text().splitlines()[0]
        stream = None

        if kind == "// $ok":
            stream = "stdout"
        elif kind == "// $fail":
            stream = "stderr"
        else:
            print(f"[SKIP] {script} kind was not set")
            continue

        result = run([wi, script], capture_output=True, text=True)
        actual = getattr(result, stream)
        expected = out.read_text()

        if (actual != expected):
            print(f"[FAIL] {script} has unexpected output!")
            print(f"--- expected ---\n{expected}")
            print(f"--- actual ---\n{actual}")
            global failed
            failed = 1
        else:
            print(f"[PASS] {script} passed!")


test("./test/bug")
test("./test/lang", recursive=True)

exit(failed)
