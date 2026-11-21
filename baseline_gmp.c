#include <stdlib.h>
#include <string.h>

#include <gmp.h>

#include "bigint.h"

// -----------------------------------------------------------------------------
// GMP baseline (https://gmplib.org): hand-tuned assembly, Karatsuba, Toom-Cook,
// FFT multiplication; serial.
//
// GMP limbs are 64-bit. On a little-endian machine, n 32-bit limbs with n even
// are exactly n/2 GMP limbs: same bytes, no conversion. Odd n goes through
// zero-padded copies.
// -----------------------------------------------------------------------------

#if GMP_LIMB_BITS != 64
#error "64-bit GMP limbs required"
#endif

static limb_t* padded_copy(const limb_t* x, size_t n, size_t total) {
    limb_t* p = calloc(total, sizeof(limb_t));
    memcpy(p, x, n * sizeof(limb_t));
    return p;
}

limb_t gmp_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n) {
    if (n % 2 == 0)
        return (limb_t)mpn_add_n((mp_limb_t*)c, (const mp_limb_t*)a, (const mp_limb_t*)b, n / 2);

    // odd n: add n+1 limbs, the carry lands in limb n
    limb_t* pa = padded_copy(a, n, n + 1);
    limb_t* pb = padded_copy(b, n, n + 1);
    limb_t* pc = calloc(n + 1, sizeof(limb_t));
    mpn_add_n((mp_limb_t*)pc, (const mp_limb_t*)pa, (const mp_limb_t*)pb, (n + 1) / 2);
    memcpy(c, pc, n * sizeof(limb_t));
    limb_t carry = pc[n];
    free(pa); free(pb); free(pc);
    return carry;
}

void gmp_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n) {
    if (n % 2 == 0) {
        mpn_mul_n((mp_limb_t*)c, (const mp_limb_t*)a, (const mp_limb_t*)b, n / 2);
        return;
    }

    // odd n: multiply n+1 limbs, the top 2 limbs of the 2n+2 are zero
    limb_t* pa = padded_copy(a, n, n + 1);
    limb_t* pb = padded_copy(b, n, n + 1);
    limb_t* pc = calloc(2 * n + 2, sizeof(limb_t));
    mpn_mul_n((mp_limb_t*)pc, (const mp_limb_t*)pa, (const mp_limb_t*)pb, (n + 1) / 2);
    memcpy(c, pc, 2 * n * sizeof(limb_t));
    free(pa); free(pb); free(pc);
}
