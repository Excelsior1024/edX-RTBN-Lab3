#include "dense_matrix.h"
#include "smart_mul_matrix.h"

#include <errno.h>
#include <stdbool.h>

//TODO: Add types, data and functions as required.

/** Return a newly allocated matrix with all entries in consecutive
 *  memory locations (row-major layout).  All entries in the newly
 *  created matrix are initialized to 0.  The return'd matrix uses
 *  a smart multiplication algorithm to avoid caching issues;
 *  specifically, transpose the multiplier and use a modified
 *  multiplication algorithm with the transposed multiplier.
 *
 *  Set *err to EINVAL if nRows or nCols <= 0, to ENOMEM if not enough
 *  memory.
 */
SmartMulMatrix *newSmartMulMatrix(int nRows, int nCols, int *err)
{
  return NULL;
}

/** Return implementation of functions for a smart multiplication
 *  matrix; these functions can be used by sub-classes to inherit
 *  behavior from this class.
 */
const SmartMulMatrixFns *
getSmartMulMatrixFns(void)
{
  return NULL;
}
