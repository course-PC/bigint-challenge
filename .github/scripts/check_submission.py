#!/usr/bin/env python3
"""
Push check for a bigint-challenge submission.

  usage: check_submission.py SUBMISSION_DIR HARNESS_DIR BUILD_DIR

  1. header:     group members (Surname; Name; 8-digit ID), key ideas
  2. rules:      big-integer libraries, threads outside OpenMP, constructors, reference still in use
  3. files:      harness files modified, extra files
  4. build:      original harness + student my_bigint.c, gcc build and link
  5. symbols:    my_bigint.o defines no library / harness symbols
  6. tests:      exact results on small inputs, every input kind, 1 and 3 threads (no timing)

Errors fail the check, warnings do not. Output: GitHub annotations + job summary.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# ----------------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------------

STUDENT = "my_bigint.c"
PROTECTED = ["main.c", "bigint.h", "reference.c", "baseline_gmp.c", "Makefile"]
ALLOWED_EXTRA = {"README.md", ".gitignore"}

MAX_MEMBERS = 3
PLACEHOLDER = "Surname; Name; Student ID"
ID_RE = re.compile(r"\d{8}")
NAME_RE = re.compile(r"[^\W\d_]+(?:[ '’.\-]+[^\W\d_]+)*\.?")  # Unicode letters, spaces, ' . -

FORBIDDEN = [
    (re.compile(r"\bgmp(xx)?\.h\b|\b(mpn|mpz|mpf|mpq)_\w+|\b__gmp\w*"), "GMP is not allowed in your code"),
    (re.compile(r"\bgmp_(add|mul)\b"), "calling the GMP baseline is not allowed"),
    (re.compile(r"\b(flint|ntl|tommath|boost)\b", re.I), "big-integer libraries are not allowed"),
    (re.compile(r"\bpthread\.h\b|\bthreads\.h\b|\bpthread_create\b|\bthrd_create\b|\bfork\s*\("),
     "threads outside OpenMP are not allowed"),
    (re.compile(r"__attribute__\s*\(\(\s*(constructor|destructor)"), "constructors/destructors are not allowed"),
]

# strong definitions that would replace library or harness functions
BANNED_SYMBOL = re.compile(
    r"^(main|ref_\w+|gmp_\w+|__gmp\w*|mpn_\w+|mpz_\w+|omp_\w+|GOMP_\w+|pthread_\w+"
    r"|malloc|calloc|realloc|free|aligned_alloc|posix_memalign|memcpy|memset|memmove|memcmp|qsort)$"
)

CC = os.environ.get("CC", "gcc")

# small tests: (op, n), every input kind, 1 and 3 threads
TESTS = [("add", n) for n in (1, 2, 3, 64, 1000, 100003, 2000001)] + \
        [("mul", n) for n in (1, 2, 3, 47, 48, 49, 64, 1000, 1023, 4097)]
INPUTS = ["random", "ones", "ripple", "prop", "sparse"]
THREADS = [1, 3]
TEST_TIMEOUT = 60  # seconds per run

# ----------------------------------------------------------------------------
# Reporting
# ----------------------------------------------------------------------------

results = []  # (level, check, message, line)
logs = {}     # check -> output


def report(level, check, message, line=None):
    results.append((level, check, message, line))
    loc = f"file={STUDENT},line={line}," if line else ""
    text = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    print(f"::{level} {loc}title={check}::{text}")


def write_summary():
    icon = {"error": "❌", "warning": "⚠️", "notice": "ℹ️"}
    checks = ["Header", "Rules", "Files", "Build", "Symbols", "Tests"]
    out = ["## Submission check", "", "| check | result |", "|---|---|"]
    for c in checks:
        items = [r for r in results if r[1] == c]
        if not items:
            out.append(f"| {c} | ✅ |")
        for level, _, msg, line in items:
            where = f" (line {line})" if line else ""
            out.append(f"| {c} | {icon[level]} {msg.splitlines()[0]}{where} |")
    for c, log in logs.items():
        out += ["", f"<details><summary>{c} log</summary>", "", "```", log[-6000:], "```", "", "</details>"]
    errors = sum(r[0] == "error" for r in results)
    out += ["", "**Result: " + ("FAIL" if errors else "PASS") + "**", ""]
    text = "\n".join(out)
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as f:
            f.write(text)
    else:
        print(text)


# ----------------------------------------------------------------------------
# Source helpers
# ----------------------------------------------------------------------------

def strip_comments(src):
    """Blank out comments and string literals, keep line numbers."""
    out, i, n = [], 0, len(src)
    while i < n:
        c, nxt = src[i], src[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            while i < n and src[i] != "\n":
                i += 1
        elif c == "/" and nxt == "*":
            i += 2
            while i < n and not (src[i] == "*" and i + 1 < n and src[i + 1] == "/"):
                out.append("\n" if src[i] == "\n" else " ")
                i += 1
            i += 2
        elif c in "\"'":
            q, i = c, i + 1
            out.append(" ")
            while i < n and src[i] != q and src[i] != "\n":
                i += 2 if src[i] == "\\" else 1
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


# ----------------------------------------------------------------------------
# 1. Header
# ----------------------------------------------------------------------------

def check_header(src):
    lines = src.splitlines()
    group = next((i for i, l in enumerate(lines) if re.match(r"\s*//\s*Group\b", l)), None)
    if group is None:
        report("error", "Header", "group header not found: restore it from the template (\"// Group ...\")", 1)
        return

    # members
    members, i = [], group + 1
    while i < len(lines) and lines[i].lstrip().startswith("//"):
        text = lines[i].lstrip()[2:].strip()
        if not text or text.lower().startswith("key ideas"):
            break
        if text != PLACEHOLDER:
            parts = [p.strip() for p in text.split(";")]
            if len(parts) != 3:
                report("error", "Header", f"expected 'Surname; Name; Student ID', found '{text}'", i + 1)
            else:
                surname, name, sid = parts
                if not NAME_RE.fullmatch(surname):
                    report("error", "Header", f"invalid surname '{surname}'", i + 1)
                elif not NAME_RE.fullmatch(name):
                    report("error", "Header", f"invalid name '{name}'", i + 1)
                elif not ID_RE.fullmatch(sid):
                    report("error", "Header", f"student ID must be 8 digits, found '{sid}'", i + 1)
                else:
                    members.append((surname, name, sid, i + 1))
        i += 1

    if not members and not any(r[1] == "Header" for r in results):
        report("error", "Header", "fill in at least one group member: 'Surname; Name; Student ID'", group + 2)
    if len(members) > MAX_MEMBERS:
        report("error", "Header", f"{len(members)} members, at most {MAX_MEMBERS} allowed", members[-1][3])
    seen = set()
    for _, _, sid, ln in members:
        if sid in seen:
            report("error", "Header", f"duplicate student ID {sid}", ln)
        seen.add(sid)

    # key ideas
    ideas_at = next((j for j, l in enumerate(lines) if re.match(r"\s*//\s*Key ideas", l, re.I)), None)
    filled = 0
    if ideas_at is not None:
        j = ideas_at + 1
        while j < len(lines) and lines[j].lstrip().startswith("//") and not re.match(r"\s*//\s*=+\s*$", lines[j]):
            m = re.match(r"\s*//\s*-\s*(.*)$", lines[j])
            if m and m.group(1).strip() not in ("", "..."):
                filled += 1
            j += 1
    if not 3 <= filled <= 4:
        report("warning", "Header", f"{filled} key ideas filled in, 3-4 expected by the deadline",
               (ideas_at or group) + 1)

    return members


# ----------------------------------------------------------------------------
# 2. Rules
# ----------------------------------------------------------------------------

def check_rules(src):
    code = strip_comments(src)
    for rx, msg in FORBIDDEN:
        m = rx.search(code)
        if m:
            report("error", "Rules", msg, line_of(code, m.start()))
    if not re.search(r'#\s*include\s*"bigint\.h"', src):
        report("error", "Rules", '#include "bigint.h" missing: it declares the functions you implement', 1)
    for fn in ("ref_add", "ref_mul"):
        m = re.search(rf"\b{fn}\b", code)
        if m:
            report("warning", "Rules", f"still calling the reference {fn}(): replace it with your implementation",
                   line_of(code, m.start()))


# ----------------------------------------------------------------------------
# 3. Files
# ----------------------------------------------------------------------------

def check_files(sub, harness):
    for f in PROTECTED:
        mine, orig = sub / f, harness / f
        if not mine.exists():
            report("notice", "Files", f"{f} missing (the original is used anyway)")
        elif mine.read_bytes() != orig.read_bytes():
            report("warning", "Files", f"{f} modified: it is replaced with the original at grading")
    extra = []
    for p in sorted(sub.rglob("*")):
        rel = p.relative_to(sub)
        if p.is_file() and rel.parts[0] not in (".git", ".github") \
                and str(rel) not in set(PROTECTED) | {STUDENT} | ALLOWED_EXTRA:
            extra.append(str(rel))
    if extra:
        shown = ", ".join(extra[:5]) + (", ..." if len(extra) > 5 else "")
        report("notice", "Files", f"only {STUDENT} is graded, ignored: {shown}")


# ----------------------------------------------------------------------------
# 4. Build
# ----------------------------------------------------------------------------

def run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return p.returncode, p.stdout


def check_build(sub, harness, build):
    shutil.rmtree(build, ignore_errors=True)
    build.mkdir(parents=True)
    for f in PROTECTED:
        shutil.copy(harness / f, build / f)
    shutil.copy(sub / STUDENT, build / STUDENT)

    rc, out = run(["make", "bigint", f"CC={CC}"], build)
    logs["Build"] = out
    if rc == 0:
        return True

    # compiler errors in the student file -> annotations
    found = False
    for m in re.finditer(r"my_bigint\.c:(\d+):\d+: (?:fatal )?error: (.*)", out):
        report("error", "Build", m.group(2).strip(), int(m.group(1)))
        found = True
    for fn in sorted(set(re.findall(r"undefined reference to `(my_add|my_mul)'", out))):
        report("error", "Build", f"{fn}() not found: keep the signature declared in bigint.h")
        found = True
    for name in sorted(set(re.findall(r"multiple definition of `([^']+)'", out))):
        report("error", "Build", f"'{name}' is defined twice: do not redefine library or harness functions")
        found = True
    if not found:
        report("error", "Build", "build failed, see the build log in the job summary")
    return False


# ----------------------------------------------------------------------------
# 5. Symbols
# ----------------------------------------------------------------------------

def check_symbols(build):
    rc, out = run([CC, "-O3", "-std=c11", "-fopenmp", "-c", STUDENT, "-o", "my_bigint.o"], build)
    if rc != 0:
        return  # compile errors are already reported by the build check
    rc, out = run(["nm", "-g", "--defined-only", "my_bigint.o"], build)
    names = [l.split()[-1] for l in out.splitlines() if len(l.split()) >= 3 and l.split()[1] in "TDBRCV"]
    for name in names:
        if BANNED_SYMBOL.match(name):
            report("error", "Symbols", f"my_bigint.c defines '{name}': only your own functions are allowed")


# ----------------------------------------------------------------------------
# 6. Tests
# ----------------------------------------------------------------------------

def check_tests(build):
    binary = build / "bigint"
    failed = []
    for op, n in TESTS:
        for inp in INPUTS:
            for t in THREADS:
                cmd = [str(binary), op, str(n), "1", inp, "2"]
                try:
                    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                       timeout=TEST_TIMEOUT, env=dict(os.environ, OMP_NUM_THREADS=str(t)))
                    ok, why = "pass=1" in p.stdout, next((l for l in p.stdout.splitlines() if "FAIL" in l), "")
                    if not ok and not why:
                        why = f"crashed (exit {p.returncode})"
                except subprocess.TimeoutExpired:
                    ok, why = False, f"timeout ({TEST_TIMEOUT} s)"
                if not ok:
                    failed.append(f"OMP_NUM_THREADS={t} ./bigint {op} {n} 1 {inp}: {why}")
    total = len(TESTS) * len(INPUTS) * len(THREADS)
    if failed:
        logs["Tests"] = "\n".join(failed)
        report("error", "Tests", f"{len(failed)} of {total} small tests failed, first: {failed[0]}")
    else:
        print(f"{total} small tests passed")


# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    sub, harness, build = (Path(a).resolve() for a in sys.argv[1:])

    student = sub / STUDENT
    if not student.exists():
        report("error", "Files", f"{STUDENT} not found at the repository root")
    else:
        src = student.read_text(encoding="utf-8", errors="replace")
        check_header(src)
        check_rules(src)
        check_files(sub, harness)
        if check_build(sub, harness, build):
            check_tests(build)
        check_symbols(build)

    write_summary()
    sys.exit(1 if any(r[0] == "error" for r in results) else 0)


if __name__ == "__main__":
    main()
