#include "mat_base.h"

/** Standard matrix multiplication using regular 2-dimensional matrices */
void
naive_matmul(int n1, int n2, int n3,
             CONST MatrixBaseType a[n1][n2],
             CONST MatrixBaseType b[n2][n3], MatrixBaseType c[n1][n3], int *err)
{
  for (int i = 0; i < n1; i++) {
    for (int j = 0; j < n3; j++) {
      c[i][j] = 0;
      for (int k = 0; k < n2; k++) c[i][j] += a[i][k]*b[k][j];
    }
  }
}
