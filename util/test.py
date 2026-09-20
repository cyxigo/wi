#!/bin/python3
from pathlib import Path
from subprocess import run

# runs every test/bug and test/lang test and compares its output to the respective .wiwi file
# first line of every .wi script must be either $ok (compare stdout), $fail (compare stderr), or $skip to skip
# might be pretty silly but works nonetheless
#
# why .wiwi? because .out is git-ignored and idk i couldn't come up with something better
wi = "wi"
exit_code = 0
passed = 0
failed = 0
skipped = 0


def test(path, recursive=False):
    path = Path(path)
    scripts = path.rglob("*.wi") if recursive else path.glob("*.wi")

    global exit_code
    global passed
    global failed

    for script in sorted(scripts):
        out = script.with_suffix(".wiwi")

        def skip(msg):
            print(f"[SKIP] {script} {msg}")
            global skipped
            skipped += 1

        kind = script.read_text().splitlines()[0]
        stream = None

        if kind == "// $ok":
            stream = "stdout"
        elif kind == "// $fail":
            stream = "stderr"
        elif kind == "// $skip":
            skip("$skip")
            continue
        else:
            skip("kind was not set")
            continue

        if not out.is_file():
            skip("has no respective .wiwi file")
            continue

        result = run([wi, script], capture_output=True, text=True)
        actual = getattr(result, stream)
        expected = out.read_text()

        if (actual != expected):
            print(f"[FAIL] {script} has unexpected output!")
            print(f"--- expected ---\n{expected}")
            print(f"--- actual ---\n{actual}")

            exit_code = 1
            failed += 1
        else:
            print(f"[PASS] {script} passed!")
            passed += 1


test("./test/bug")
test("./test/lang", recursive=True)
print(f"[RESULT] {passed} passed, {failed} failed, {skipped} skipped")
exit(exit_code)
