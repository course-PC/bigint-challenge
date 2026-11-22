# Big integers, fast

Cryptography, computer algebra and record computations of π all rest on arithmetic with *huge* integers: millions of digits. Your job: make the two oldest algorithms you know, **addition** and **multiplication**, run fast on a multicore CPU with OpenMP.

## Getting started

One member of the group joins at **https://github.com/course-PC/bigint-challenge-join**: this creates your group's private repository, `bigint-<username of that member>`, and invites the others.

```
git clone https://github.com/course-PC/bigint-<username>
cd bigint-<username>
make                                   # needs GMP, see "Build and run"
./bigint add 10000000 1                # 10 million limbs, seed 1
./bigint mul 4096 1                    # 4096 limbs
```

Then edit `my_bigint.c`, rebuild, run again, push. The starter calls the plain reference, so it is correct (and slow) from the first build.

## Problem

A big integer is an array of $`n`$ **limbs**: 32-bit unsigned digits in base $`B = 2^{32}`$, least significant first. The array `a` holds the number

```math
a = a_0 + a_1 B + a_2 B^2 + \dots + a_{n-1} B^{n-1}, \qquad 0 \le a_i < B .
```

- **Level 1 – addition.** Given two numbers `a` and `b`, $`n`$ limbs each, compute the $`n`$ limbs of $`a + b`$ and the carry out (0 or 1).
- **Level 2 – multiplication.** Given `a` and `b`, $`n`$ limbs each, compute the $`2n`$ limbs of $`a \times b`$.

### Example with B = 10: addition

Same rules, but each limb is a decimal digit, least significant first:

```
a = [9, 9, 7, 2]          2799
b = [1, 0, 2, 1]        + 1201
c = [0, 0, 0, 4]        = 4000     carry out 0
```

Digit by digit: 9 + 1 = 10, write 0 and carry 1; 9 + 0 + 1 = 10, write 0 and carry 1; 7 + 2 + 1 = 10, write 0 and carry 1; 2 + 1 + 1 = 4. One carry ripples from the first digit to the last. When the sum needs one more limb, it comes out as the carry:

```
a = [9, 9, 9, 9]          9999
b = [1, 0, 0, 0]        +    1
c = [0, 0, 0, 0]        = 0000     carry out 1   (10000)
```

### Example with B = 10: multiplication

The schoolbook way: multiply `a` by each limb of `b`, shift, add up. The result has $`2n`$ limbs.

```
a       = [9, 9, 7]                  799
b       = [3, 2, 1]                × 123
a·3     = [7, 9, 3, 2]              2397
a·2·10  = [0, 8, 9, 5, 1]          15980
a·1·100 = [0, 0, 9, 9, 7]          79900
c       = [7, 7, 2, 8, 9, 0]       98277
```

## Files

| file             | content                                                      | edit? |
|------------------|--------------------------------------------------------------|-------|
| `main.c`         | input generation, timing, checking, report                   | no    |
| `bigint.h`       | limb type, the functions you write, reference and GMP        | no    |
| `reference.c`    | **plain serial reference**: the two algorithms above, easy to read | no |
| `baseline_gmp.c` | [GMP](https://gmplib.org), the fastest serial big-integer library | no |
| `Makefile`       | build                                                        | no    |
| `my_bigint.c`    | **your `my_add()` and `my_mul()`**                           | yes   |

Only `my_bigint.c` is collected; every other file is replaced with the original at grading.

## What you write

```c
limb_t my_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);  // c[0..n) + carry out
void   my_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);  // c[0..2n)
```

- `a` and `b` are read-only, `c` never overlaps them;
- **`c` is not zeroed for you**: it arrives full of garbage (`0xa5a5a5a5`), write every limb;
- `n` is any value ≥ 1 (not necessarily even, nor a power of 2);
- the whole call is timed, allocations included;
- use as many threads as OpenMP gives you (`omp_get_max_threads()`), do not hard-code a number;
- you may add any helper functions, static data, temporary buffers inside `my_bigint.c`.

## Rules

- OpenMP for parallelism; no big-integer libraries in your code (GMP is only used by the harness).
- **Any algorithm is allowed**, as long as the result is exact, bit for bit.
- Every call computes from `a` and `b`: inputs change at every call, nothing can be reused across calls.
- Code runs only inside `my_add()` / `my_mul()`: do not redefine library or harness functions (GMP's included), no global constructors, no background threads.
- The number of threads is set by the harness (`OMP_NUM_THREADS`).

## Build and run

You need gcc with OpenMP and GMP:

| system        | setup |
|---------------|-------|
| Ubuntu/Debian | `sudo apt install build-essential libgmp-dev`, then `make` |
| macOS         | Apple's clang has no OpenMP: `brew install gcc gmp`, then `make CC=gcc-14 GMP_DIR=$(brew --prefix gmp)` (use the version `brew` installed) |
| Windows       | use WSL (Ubuntu), then as Ubuntu |

```
./bigint add|mul n seed [input] [reps]
./bigint add 100000000 1                 # 10^8 limbs: needs ~2 GB of RAM
./bigint mul 16384 1 ones
OMP_NUM_THREADS=8 ./bigint mul 16384 1
BIGINT_REF=1 ./bigint mul 4096 1         # also time the plain reference (O(n^2): slow above ~16384)
```

`input` is one of:

| input    | what it is | what it stresses |
|----------|------------|------------------|
| `random` | uniform limbs | the common case |
| `ones`   | every limb $`B-1`$ | the largest products and sums: overflow |
| `ripple` | $`a = B^n - 1`$, $`b = 1`$ | one carry that crosses the whole number |
| `prop`   | $`a_i + b_i = B - 1`$ almost everywhere | long carry chains that cross any chunk boundary |
| `sparse` | 99% zero limbs | uneven work |

The program runs GMP and your functions on fresh inputs at every repeat, checks every result exactly (against GMP, and modulo a prime), and reports median times, the speedup over GMP and, for the addition, the bandwidth reached. Exit code is nonzero if your result is wrong. For example, the starter (the plain reference) on a laptop:

```
$ ./bigint mul 4096 1 random 3
op      mul  n=4096 limbs (16.4 KB per operand)  input=random  seed=1  reps=3  threads=2
gmp          0.217 ms
student      9.441 ms  PASS
speedup 0.023x vs gmp
```

A wrong result prints what went wrong, e.g. `FAIL: wrong result (differs from GMP)`, how many limbs are wrong and the first one, with its expected value.

## Benchmarks

Grading runs on an **AMD EPYC 7302** (16 cores) with **16 threads**, one per core, on every kind of input:

| level          | n (limbs)              | operand size        |
|----------------|------------------------|---------------------|
| addition       | 10⁷, 10⁸               | 40 MB, 400 MB       |
| multiplication | 4096, 8192, 16384      | 16 KB, 32 KB, 64 KB |

```
OMP_NUM_THREADS=16 OMP_PLACES=cores OMP_PROC_BIND=spread ./bigint mul 8192 1
```

Develop on smaller sizes and fewer threads if your machine is smaller; correctness does not depend on the size.

## Evaluation

**Correctness first**: exact on all hidden inputs (every input kind, odd sizes included). A wrong result anywhere invalidates the level.

Then 3 points:

- **Performance, 2 points**, from measurements and profiling:
  - **reach the bar and both points are guaranteed**: geometric mean of your speedups over GMP of **at least 1.5×** on the addition (all its sizes and inputs) **and at least 0.5×** on the multiplication (all its sizes and inputs);
  - otherwise, judged from the profiles: speedups over GMP, how your code scales from 1 to 16 threads, how close your addition gets to the machine's memory bandwidth.
- **Code, 1 point**, from reading it: the intelligence of your parallel implementation. Clear parallel patterns, used with the right intent and in the right place; no wasted parallelism (threads that cost more than they give); no trivial chokepoints (serial sections, locks or barriers where the work could flow). Your 3–4 key ideas must match your code.

## Food for thought

- The reference adder is a *ripple-carry adder*: every limb waits for the carry of the one before it. Hardware designers stopped waiting decades ago.
- Look up how a *Dadda multiplier* adds up its partial products, and notice when it lets carries propagate.
- GMP is serial, but it is very good: for large multiplications it does not use the schoolbook algorithm at all.

## Submission

Submissions open on **24 November 2026** and close on **8 December 2026, 23:59** (Italian time).

`my_bigint.c` only, with the header filled in:

- one line per group member, exactly `Surname; Name; Student ID` (8-digit ID), e.g. `Rossi; Mario; 12345678`; delete the unused placeholder lines;
- 3–4 bullets on your main ideas.

Your submission is `my_bigint.c` on the `main` branch at the deadline; after that the repository becomes read-only.

Every push that changes `my_bigint.c` is checked automatically (Actions tab, ✅ / ❌ on the commit; run it by hand with *Run workflow*): header filled in, rules, build, and exact results on small inputs of every kind with 1 and 3 threads. The check machine is small and shared: it measures no speed, benchmark on your own machine.
