#ifndef _MAT_BASE_H
#define _MAT_BASE_H

/** Common matrix declarations needed by modules, client and server. */

/** The type of each matrix entry */
typedef int MatrixBaseType;

/** Use this macro to handle MatrixBaseType in printf(), scanf() routines */
#define MATRIX_BASE_TYPE_FMT "%8d"

/* Input matrices should be declared const MatrixBaseType[][], but
 * doing so results in warnings on gcc 4.9.2-10.  Hence this macro.
 */
#if __GNUC__ == 4 && __GNUC_MINOR__ == 9 && __GNUC_PATCHLEVEL__ == 2
#define CONST
#else
 #define CONST const
#endif

/** Typedef for function provided by matrix multiplication modules */
typedef void MatrixMulFn(int n1, int n2, int n3,
                         CONST MatrixBaseType a[n1][n2],
                         CONST MatrixBaseType b[n2][n3],
                         MatrixBaseType c[n1][n3], int *err);


#endif //ifndef _MAT_BASE_H
