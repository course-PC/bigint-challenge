#pragma once

#include <stddef.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Big integers: arrays of 32-bit limbs, base B = 2^32, least significant first
//   A = a[0] + a[1] B + a[2] B^2 + ... + a[n-1] B^(n-1)
// -----------------------------------------------------------------------------

typedef uint32_t limb_t;

// ===============================================
// YOUR CODE's entry points => implement in my_bigint.c
// ===============================================

// c[0..n) = low n limbs of a + b, returns the carry out (0 or 1)
limb_t my_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);

// c[0..2n) = a * b
void my_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);

// -------------------------------------------------------
// Reference: plain serial, easy to read (reference.c)
// -------------------------------------------------------

limb_t ref_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);
void ref_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);

// -------------------------------------------------------
// Baseline: GMP, the fastest serial library (baseline_gmp.c)
// -------------------------------------------------------

limb_t gmp_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n);
void gmp_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n);
