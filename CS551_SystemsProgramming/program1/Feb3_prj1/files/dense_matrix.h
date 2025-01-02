#ifndef _SIMPLE_MATRIX_H
#define _SIMPLE_MATRIX_H

#include "matrix.h"

typedef struct {
  MatrixFns;     //-fms-extensions inserts MatrixFns fields into struct
} DenseMatrixFns;

typedef struct {
  Matrix;        //-fms-extensions inserts Matrix fields into struct
} DenseMatrix;

/** Return a newly allocated matrix with all entries in consecutive
 *  memory locations (row-major layout).  All entries in the newly
 *  created matrix are initialized to 0.  Set *err to EINVAL if nRows
 *  or nCols <= 0, to ENOMEM if not enough memory.
 */
DenseMatrix *newDenseMatrix(int nRows, int nCols, int *err);

/** Return implementation of functions for a dense matrix; these functions
 *  can be used by sub-classes to inherit behavior from this class.
 */
const DenseMatrixFns *getDenseMatrixFns(void);

#endif //ifndef _SIMPLE_MATRIX_H
