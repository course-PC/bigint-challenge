#pragma once

#include <stddef.h>
#include <stdint.h>

// -----------------
// Big integer limbs
// -----------------

// type of each limb in the big integer:
// a = a[0] + a[1] B + a[2] B^2 + ... + a[n-1] B^(n-1)
// => limb_t a[n]
// => base B = 2^32
// => least significant first
typedef uint32_t limb_t;

// ====================================================
// YOUR CODE's entry points => implement in my_bigint.c
// ====================================================

// c[0..n) = low n limbs of a + b, returns the carry out (0 or 1)
limb_t my_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);

// c[0..2n) = a * b
void my_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);

// -------------------------------------
// Reference (plain serial, reference.c)
// -------------------------------------

limb_t ref_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);
void ref_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);

// --------------------------------------
// Baseline (GMP, serial, baseline_gmp.c)
// --------------------------------------

limb_t gmp_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);
void gmp_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);
