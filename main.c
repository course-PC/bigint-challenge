#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <omp.h>

#include "bigint.h"

// -----------------------------------------------------------------------------
// Inputs: deterministic from (seed, index), regenerated at every repeat
//   random  uniform limbs
//   ones    every limb B-1 (largest values: largest column sums in a product)
//   ripple  a = B^n - 1, b = 1: one carry crosses the whole number
//   prop    a[i] + b[i] = B-1 (the limb propagates a carry), except 1 limb in 65536 that
//           generates a carry (3/4) or kills it (1/4): long carry chains across the number
//   sparse  99% zero limbs
// -----------------------------------------------------------------------------

enum Input { IN_RANDOM, IN_ONES, IN_RIPPLE, IN_PROP, IN_SPARSE };
static const char* INPUT_NAMES[] = {"random", "ones", "ripple", "prop", "sparse"};

static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static void generate(limb_t* a, limb_t* b, size_t n, int input, uint64_t seed) {
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; ++i) {
        const uint64_t h = splitmix64(seed ^ splitmix64(i));
        const limb_t x = (limb_t)h, y = (limb_t)(h >> 32);
        switch (input) {
            case IN_RANDOM: a[i] = x; b[i] = y; break;
            case IN_ONES:   a[i] = 0xFFFFFFFFu; b[i] = 0xFFFFFFFFu; break;
            case IN_RIPPLE: a[i] = 0xFFFFFFFFu; b[i] = (i == 0); break;
            case IN_PROP:
                if ((h >> 48) & 0xFFFF) { a[i] = x; b[i] = ~x; }                                   // propagate
                else if ((h >> 46) & 3) { a[i] = x | 0x80000000u; b[i] = y | 0x80000000u; }        // generate
                else                    { a[i] = x & 0x7FFFFFFFu; b[i] = y & 0x7FFFFFFFu; }        // kill
                break;
            case IN_SPARSE: a[i] = (x % 100 == 0) ? y : 0; b[i] = (y % 100 == 0) ? x : 0; break;
        }
    }
}

// -----------------------------------------------------------------------------
// Checks
//   exact:  result == GMP result, limb by limb
//   mod p:  the result is also verified modulo p = 2^61 - 1, independently of GMP
//   input:  a and b unchanged
// -----------------------------------------------------------------------------

#define P61 ((1ULL << 61) - 1)

static uint64_t mulmod61(uint64_t x, uint64_t y) {
    const unsigned __int128 t = (unsigned __int128)x * y;
    uint64_t r = (uint64_t)(t & P61) + (uint64_t)(t >> 61);
    return r >= P61 ? r - P61 : r;
}

static uint64_t addmod61(uint64_t x, uint64_t y) {
    uint64_t r = x + y;
    return r >= P61 ? r - P61 : r;
}

static uint64_t powmod61(uint64_t x, uint64_t e) {
    uint64_t r = 1;
    for (; e; e >>= 1, x = mulmod61(x, x))
        if (e & 1) r = mulmod61(r, x);
    return r;
}

// value of x[0..n) mod p, Horner from the most significant limb
static uint64_t value_mod61(const limb_t* x, size_t n) {
    uint64_t r = 0;
    for (size_t i = n; i-- > 0;)
        r = addmod61(mulmod61(r, 1ULL << 32), x[i]);
    return r;
}

static uint64_t fingerprint(const limb_t* x, size_t n) {
    uint64_t h = 0;
    #pragma omp parallel for reduction(^ : h) schedule(static)
    for (size_t i = 0; i < n; ++i)
        h ^= splitmix64(i * 0x100000001B3ULL + x[i]);
    return h;
}

// -----------------------------------------------------------------------------
// Benchmark: one warm-up repeat + timed repeats, every repeat checked
// -----------------------------------------------------------------------------

enum Op { OP_ADD, OP_MUL };

typedef struct {
    int pass;
    double t_gmp, t_student, t_ref; // median ms, t_ref < 0 if not run
} Result;

static int cmp_double(const void* x, const void* y) {
    const double a = *(const double*)x, b = *(const double*)y;
    return (a > b) - (a < b);
}

static double median(double* t, int k) {
    qsort(t, k, sizeof(double), cmp_double);
    return t[k / 2];
}

static Result bench(int op, size_t n, int input, uint64_t seed, int reps, int with_ref) {
    // NOTE: GMP always gets an even length ne (zero pad limb when n is odd), so its timing
    //       never includes padding copies; the student gets exactly n limbs
    const size_t ne = n + (n & 1);
    const size_t out = op == OP_ADD ? n : 2 * n;
    limb_t* a = calloc(ne, sizeof(limb_t));
    limb_t* b = calloc(ne, sizeof(limb_t));
    limb_t* c = malloc(out * sizeof(limb_t));                        // student
    limb_t* e = malloc((op == OP_ADD ? ne : 2 * ne) * sizeof(limb_t)); // expected (GMP)
    double* tg = malloc(reps * sizeof(double));
    double* ts = malloc(reps * sizeof(double));
    double* tr = malloc(reps * sizeof(double));
    Result r = {1, 0, 0, -1};

    for (int rep = -1; rep < reps; ++rep) {
        // fresh input, untimed
        generate(a, b, n, input, splitmix64(seed + (uint64_t)(rep + 1)));
        const uint64_t fa = fingerprint(a, n), fb = fingerprint(b, n);
        memset(c, 0xA5, out * sizeof(limb_t));

        // GMP: expected result + baseline time
        double t0 = omp_get_wtime();
        limb_t ce = op == OP_ADD ? gmp_add(a, b, e, ne) : (gmp_mul(a, b, e, ne), 0);
        const double t_gmp = omp_get_wtime() - t0;
        if (op == OP_ADD && ne != n) ce = e[n]; // odd n: the carry out lands in the pad limb

        // student
        t0 = omp_get_wtime();
        const limb_t cs = op == OP_ADD ? my_add(a, b, c, n) : (my_mul(a, b, c, n), 0);
        const double t_student = omp_get_wtime() - t0;

        // reference (optional, it is slow for mul)
        double t_ref = -1;
        if (with_ref) {
            limb_t* x = malloc(out * sizeof(limb_t));
            t0 = omp_get_wtime();
            if (op == OP_ADD) ref_add(a, b, x, n); else ref_mul(a, b, x, n);
            t_ref = omp_get_wtime() - t0;
            free(x);
        }

        // checks
        const char* fail = NULL;
        if (fingerprint(a, n) != fa || fingerprint(b, n) != fb)
            fail = "input modified";
        else if (cs != ce || memcmp(c, e, out * sizeof(limb_t)) != 0)
            fail = "wrong result (differs from GMP)";
        else {
            const uint64_t va = value_mod61(a, n), vb = value_mod61(b, n), vc = value_mod61(c, out);
            const uint64_t want = op == OP_ADD ? addmod61(va, vb) : mulmod61(va, vb);
            const uint64_t got = op == OP_ADD ? addmod61(vc, mulmod61(cs, powmod61(1ULL << 32, n))) : vc;
            if (got != want) fail = "wrong result (mod p check)";
        }
        if (fail) {
            size_t first = 0, wrong = 0;
            while (first < out && c[first] == e[first]) ++first;
            for (size_t i = first; i < out; ++i) wrong += c[i] != e[i];
            fprintf(stderr, "FAIL: %s (%s)\n", fail, rep < 0 ? "warm-up run" : "timed run");
            if (first < out)
                fprintf(stderr, "      %zu of %zu limbs wrong, first c[%zu] = 0x%08x, expected 0x%08x%s\n",
                        wrong, out, first, c[first], e[first],
                        c[first] == 0xA5A5A5A5u ? "  (0xa5a5a5a5 = never written: c is not zeroed for you)" : "");
            if (cs != ce)
                fprintf(stderr, "      carry out %u, expected %u\n", cs, ce);
            r.pass = 0;
            break;
        }

        if (rep >= 0) { tg[rep] = t_gmp * 1e3; ts[rep] = t_student * 1e3; tr[rep] = t_ref * 1e3; }
    }

    if (r.pass) {
        r.t_gmp = median(tg, reps);
        r.t_student = median(ts, reps);
        r.t_ref = with_ref ? median(tr, reps) : -1;
    }
    free(a); free(b); free(c); free(e); free(tg); free(ts); free(tr);
    return r;
}

// -----------------------------------------------------------------------------
// CLI
// -----------------------------------------------------------------------------

static void usage(const char* prog) {
    fprintf(stderr,
            "usage: %s add|mul n seed [input] [reps]\n"
            "  n      limbs per operand (>= 1)\n"
            "  seed   RNG seed (>= 0)\n"
            "  input  random | ones | ripple | prop | sparse   (default random)\n"
            "  reps   timed repetitions                         (default 10)\n"
            "env: OMP_NUM_THREADS   threads for your code\n"
            "     BIGINT_REF=1      also time the plain reference (slow for mul)\n", prog);
    exit(EXIT_FAILURE);
}

static int parse_size(const char* s, unsigned long long* v) {
    char* end;
    *v = strtoull(s, &end, 10);
    return *s && !*end;
}

// ----
// Main
// ----

int main(int argc, char** argv) {
    // cli args
    if (argc < 4 || argc > 6) usage(argv[0]);
    int op;
    if (!strcmp(argv[1], "add")) op = OP_ADD;
    else if (!strcmp(argv[1], "mul")) op = OP_MUL;
    else usage(argv[0]);
    unsigned long long n, seed, reps = 10;
    if (!parse_size(argv[2], &n) || n < 1 || !parse_size(argv[3], &seed)) usage(argv[0]);
    int input = IN_RANDOM;
    if (argc >= 5) {
        for (input = 0; input < 5 && strcmp(argv[4], INPUT_NAMES[input]); ++input) {}
        if (input == 5) usage(argv[0]);
    }
    if (argc == 6 && (!parse_size(argv[5], &reps) || reps < 1)) usage(argv[0]);
    const char* ref_env = getenv("BIGINT_REF");
    const int with_ref = ref_env && !strcmp(ref_env, "1");

    const double mb = n * 4.0 / 1e6;  // operand size
    printf("op      %s  n=%llu limbs (%.*f %s per operand)  input=%s  seed=%llu  reps=%llu  threads=%d\n",
           argv[1], n, mb < 1 ? 1 : 0, mb < 1 ? mb * 1e3 : mb, mb < 1 ? "KB" : "MB",
           INPUT_NAMES[input], seed, reps, omp_get_max_threads());
    fflush(stdout);

    // run
    const Result r = bench(op, (size_t)n, input, seed, (int)reps, with_ref);

    // report results
    if (!r.pass) {
        printf("student FAIL\n");
        printf("RESULT op=%s n=%llu input=%s threads=%d pass=0\n", argv[1], n, INPUT_NAMES[input], omp_get_max_threads());
        return EXIT_FAILURE;
    }
    // NOTE: add bandwidth over the minimal traffic: read a, b, write c
    const double bytes = op == OP_ADD ? 12.0 * n : 0.0;
    if (r.t_ref >= 0) printf("ref     %10.3f ms\n", r.t_ref);
    printf("gmp     %10.3f ms", r.t_gmp);
    if (bytes > 0) printf("  %7.1f GB/s", bytes / (r.t_gmp * 1e6));
    printf("\nstudent %10.3f ms", r.t_student);
    if (bytes > 0) printf("  %7.1f GB/s", bytes / (r.t_student * 1e6));
    printf("  PASS\nspeedup %.3fx vs gmp", r.t_gmp / r.t_student);
    if (r.t_ref >= 0) printf(", %.3fx vs ref", r.t_ref / r.t_student);
    printf("\n");

    printf("RESULT op=%s n=%llu input=%s threads=%d pass=1 t_gmp_ms=%.4f t_student_ms=%.4f t_ref_ms=%.4f speedup_gmp=%.4f\n",
           argv[1], n, INPUT_NAMES[input], omp_get_max_threads(), r.t_gmp, r.t_student, r.t_ref, r.t_gmp / r.t_student);
    return EXIT_SUCCESS;
}
