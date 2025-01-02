#include "mat_base.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

static const MatrixBaseType *
transpose_matrix(int n1, int n2, CONST MatrixBaseType in[n1][n2], int *err)
{
  MatrixBaseType *out = malloc(n2 * n1 * sizeof(MatrixBaseType));
  if (!out) {
    *err = errno;
    return NULL;
  }
  int n = 0;
  for (int i = 0; i < n2; i++) {
    for (int j = 0; j < n1; j++) {
      out[n++] = in[j][i];
    }
  }
  assert(n == n1 * n2);
  return out;
}


/** Matrix multiplication using regular 2-dimensional matrices.
 *  Multiplier is transposed before multiplication to make
 *  behavior more cache-friendly for large matrices.
 */
void
smart_matmul(int n1, int n2, int n3,
             CONST MatrixBaseType a[n1][n2],
             CONST MatrixBaseType b[n2][n3], MatrixBaseType c[n1][n3], int *err)
{
  CONST MatrixBaseType (*transpose)[n2] =
    (CONST MatrixBaseType (*)[n2])transpose_matrix(n2, n3, b, err);
  if (*err == 0) {
    for (int i = 0; i < n1; i++) {
      for (int j = 0; j < n3; j++) {
        c[i][j] = 0;
        for (int k = 0; k < n2; k++) c[i][j] += a[i][k]*transpose[j][k];
      }
    }
    free((void *)transpose);
  }
}
