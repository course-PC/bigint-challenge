// ============================================================================
// Group (one line per member: Surname; Name; Student ID)
//   Surname; Name; Student ID
//   Surname; Name; Student ID
//   Surname; Name; Student ID
//
// Key ideas (3-4 bullets)
//   - ...
//   - ...
//   - ...
// ============================================================================

#include <omp.h>

#include "bigint.h"

// ============================================================================
// Level 1: addition
//
// Contract:
//   - c[0..n) = low n limbs of a + b, return the carry out (0 or 1)
//   - a, b are read-only, c does not overlap a or b, c is NOT zeroed
//   - n >= 1, any value (not necessarily even or a power of 2)
//   - the whole call is timed
// ============================================================================

limb_t my_add(const limb_t* a, const limb_t* b, limb_t* c, size_t n) {
    // TODO: replace the reference with your implementation.
    return ref_add(a, b, c, n);
}

// ============================================================================
// Level 2: multiplication
//
// Contract:
//   - c[0..2n) = a * b
//   - a, b are read-only, c does not overlap a or b, c is NOT zeroed
//   - n >= 1, any value
//   - the whole call is timed
// ============================================================================

void my_mul(const limb_t* a, const limb_t* b, limb_t* c, size_t n) {
    // TODO: replace the reference with your implementation.
    ref_mul(a, b, c, n);
}
