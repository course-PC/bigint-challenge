# Big integers on a multicore CPU

## Problem

A big integer is an array of $`n`$ **limbs**, 32-bit unsigned digits in base $`B = 2^{32}`$, **least significant first**: the array `a` holds the number

```math
a = a_0 + a_1 B + a_2 B^2 + \dots + a_{n-1} B^{n-1}, \qquad 0 \le a_i < B .
```

Given `a` and `b`, $`n`$ limbs each, compute **with OpenMP**:

- **Level 1, addition**: the $`n`$ limbs of $`a + b`$ and the carry out (0 or 1);
- **Level 2, multiplication**: the $`2n`$ limbs of $`a \times b`$.

## Example with B = 10

Same rules, each limb is a decimal digit, least significant first.

**Addition**, $`n = 4`$:

```
a = [9, 9, 7, 2]          2799
b = [1, 0, 2, 1]        + 1201
c = [0, 0, 0, 4]        = 4000     carry out 0
```

One carry ripples from the first limb to the last. When the sum needs one more limb, it is the carry out: `[9, 9, 9, 9] + [1, 0, 0, 0] = [0, 0, 0, 0]`, carry out 1 (9999 + 1 = 10000).

**Multiplication**, $`n = 3`$, the schoolbook way: multiply `a` by each limb of `b`, shift, add up.

```
a       = [9, 9, 7]                  799
b       = [3, 2, 1]                × 123
a·3     = [7, 9, 3, 2]              2397
a·2·10  = [0, 8, 9, 5, 1]          15980
a·1·100 = [0, 0, 9, 9, 7]          79900
c       = [7, 7, 2, 8, 9, 0]       98277
```

The product has $`2n`$ limbs: here the top one is 0.

## Files

| file             | role                                                   | edit? |
|------------------|--------------------------------------------------------|-------|
| `main.c`         | input generation, timing, checking, report             | no    |
| `bigint.h`       | `limb_t`, your functions, reference and baseline       | no    |
| `reference.c`    | plain serial reference: the two algorithms above       | no    |
| `baseline_gmp.c` | [GMP](https://gmplib.org) baseline, the fastest serial big-integer library | no |
| `Makefile`       | build                                                  | no    |
| `my_bigint.c`    | **your `my_add()` and `my_mul()`**                     | yes   |

Only `my_bigint.c` is collected; all other files are replaced with the original ones at grading.

## What you write

```c
limb_t my_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);  // c[0..n), returns the carry out
void   my_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);  // c[0..2n)
```

- `a` and `b` are read-only, `c` never overlaps them;
- `c` is **not** zeroed: write every limb;
- `n` is any value ≥ 1, not necessarily even nor a power of 2;
- use the threads OpenMP gives you (`omp_get_max_threads()`), do not hard-code a number;
- the whole call is timed, allocations included.

You may add any helper functions, static data and temporary buffers inside `my_bigint.c`.

## Rules

- OpenMP only: no other threads, no big-integer libraries in your code (GMP is used by the harness only).
- Any algorithm is allowed, as long as the result is exact.
- Every call computes from `a` and `b`: the inputs change at every call, nothing can be reused across calls.
- Code runs only inside `my_add()` / `my_mul()`: do not redefine library or harness functions (GMP's included), no global constructors, no background threads.

## Build and run

Requires gcc with OpenMP and GMP. Ubuntu/Debian: `sudo apt install build-essential libgmp-dev`; macOS: `brew install gcc gmp`, then `make CC=gcc-14 GMP_DIR=$(brew --prefix gmp)` (Apple's clang has no OpenMP); Windows: WSL.

```
make
./bigint add|mul n seed [input] [reps]
./bigint mul 4096 1
```

The program runs GMP and your functions on fresh inputs at every repeat, checks every result exactly, and reports median time, speedup over GMP and, for the addition, bandwidth. Exit code is nonzero if your output is wrong. `OMP_NUM_THREADS` sets the threads; `BIGINT_REF=1` also times the plain reference (slow for large multiplications).

| input    | content                                    | stresses                                        |
|----------|--------------------------------------------|-------------------------------------------------|
| `random` | uniform limbs (default)                    | the common case                                 |
| `ones`   | every limb $`B-1`$                         | the largest products and sums: overflow         |
| `ripple` | $`a = B^n - 1`$, $`b = 1`$                 | one carry across the whole number               |
| `prop`   | $`a_i + b_i = B - 1`$ almost everywhere    | long carry chains across any chunk boundary     |
| `sparse` | 99% zero limbs                             | uneven work                                     |

## Benchmarks

Grading runs on an AMD EPYC 7302 (16 cores) with 16 threads, one per core, mostly at sizes like these, on every input:

| level          | n (limbs)          | operand size        |
|----------------|--------------------|---------------------|
| addition       | 10⁷, 10⁸           | 40 MB, 400 MB       |
| multiplication | 4096, 8192, 16384  | 16 KB, 32 KB, 64 KB |

Peak memory stays within ~2 GB.

```
OMP_NUM_THREADS=16 OMP_PLACES=cores OMP_PROC_BIND=spread ./bigint add 100000000 1
OMP_NUM_THREADS=16 OMP_PLACES=cores OMP_PROC_BIND=spread ./bigint mul 8192 1 ones
```

## Evaluation

1. **Correctness**: exact results on all hidden inputs, every input kind, odd sizes included; a wrong result invalidates the level.
2. **Performance**, 2 points, from measurements and profiling: guaranteed with a geometric-mean speedup over GMP of at least **1.5× on the addition** and **0.5× on the multiplication** (over each level's sizes and inputs); otherwise judged from the profiles: speedup over GMP, scaling from 1 to 16 threads, bandwidth of the addition.
3. **Code**, 1 point, from reading it: clear parallel patterns used with the right intent and in the right place, no wasted parallelism, no trivial chokepoints; the 3–4 bullets at the top of `my_bigint.c` must match the code.

## Hints

- The reference adder is a ripple-carry adder: every limb waits for the carry of the one before it. Hardware designers stopped waiting decades ago.
- Look up how a Dadda multiplier adds up its partial products, and notice when it lets carries propagate.
- GMP is serial, but very good: for large multiplications it does not use the schoolbook algorithm at all.

## Submission

Deadline: **8 December 2026, 23:59** (Italian time).

`my_bigint.c` only, with the header filled in:

- one line per group member, exactly `Surname; Name; Student ID`;
- 3–4 bullets on your main ideas.

Your submission is `my_bigint.c` on the default branch at the deadline; after that the repository becomes read-only.

Every push that changes `my_bigint.c` is checked automatically (Actions tab, ✅ / ❌ on the commit; run it by hand with *Run workflow*): header filled in, no forbidden APIs, code compiles, exact results on small inputs with 1 and 3 threads. The check machine is small and shared: it does not time your code, benchmark it yourself.
