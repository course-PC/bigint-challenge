#include <string.h>

#include "bigint.h"

// ----------------------------------------------------------------------
// Reference implementations: the algorithms from school, one limb at a time,
// written to be read and checked against, not to be fast
// ----------------------------------------------------------------------

// addition with a ripple carry: limb i needs the carry out of limb i-1
limb_t ref_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n) {
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t s = (uint64_t)a[i] + b[i] + carry; // at most 2 (B-1) + 1 < 2 B
        c[i] = (limb_t)s;                           // s mod B
        carry = s >> 32;                            // s div B, 0 or 1
    }
    return (limb_t)carry;
}

// schoolbook multiplication: for every limb of a, add a[i] * b, shifted by i limbs,
// into the result, carrying as you go
void ref_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n) {
    memset(c, 0, 2 * n * sizeof(limb_t));
    for (size_t i = 0; i < n; ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < n; ++j) {
            // (B-1)^2 + (B-1) + (B-1) = B^2 - 1: always fits in 64 bits
            uint64_t t = (uint64_t)a[i] * b[j] + c[i + j] + carry;
            c[i + j] = (limb_t)t;
            carry = t >> 32;
        }
        c[i + n] = (limb_t)carry;
    }
}
