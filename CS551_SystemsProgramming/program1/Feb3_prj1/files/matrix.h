#ifndef _MATRIX_H
#define _MATRIX_H

#include <stddef.h>  //for size_t

/** Abstract interface to a 2-D matrix with zero-origin row and column
 *  indexes.
 */

/** The type of each matrix entry */
typedef int MatrixBaseType;

//Forward declaration of incomplete struct
typedef struct MatrixFns MatrixFns;

/** There are different matrix implementations but all concrete matrix
 *  implementation structs will contain an initial field which is a
 *  pointer to a struct containing pointers to the different matrix
 *  functions.
 *
 *  Hence a concrete matrix will be represented as a pointer to a
 *  different struct, but this struct must have MatrixFns *fns as its
 *  initial member.  The pointer to the concrete struct type can be
 *  converted to the abstract struct Matrix pointer and back again
 *  without problems as per section 6.3.2.3, #7 of the n1570 C
 *  standard (pg. 56, of 4/12/2011 Committee Draft):
 *
 *      A pointer to an object type may be converted to a pointer to a
 *      different object type.  If the resulting pointer is not
 *      correctly aligned for the referenced type, the behavior is
 *      undefined.  Otherwise, when converted back again, the result
 *      shall compare equal to the original pointer.
 *
 *  Additionally, it should be possible to use the functions in fns
 *  simply via the Matrix interface.  Specifically, section 6.7.2.1
 *  #15 of the n1570 C standard (pg. 115, of 4/12/2011 Committee
 *  Draft):
 *
 *      A pointer to a structure object, suitably converted, points to
 *      its initial member (...), and vice versa.  There may be
 *      unnamed padding within a structure object, but not at its
 *      beginning
 *
 */
typedef struct {
  const MatrixFns *fns;
} Matrix;

/** All matrix implementations will implement the interface represented
 *  by struct MatrixFns: a struct of function pointers.
 *
 *  Note that the this matrix represents the receiver (in OOP
 *  terminology).  The code for each matrix function will cast the
 *  this pointer to the pointer corresponding to the concrete type.
 *  However, non-reflective code will access any other or result
 *  matrices only as abstract struct Matrix pointers (that is the code
 *  will not assume any specific implementation for other or result
 *  matrices).
 *
 *  A matrix is in an invalid state if its # of rows and cols are
 *  not positive.
 *
 *  All matrix functions have an *err argument used to return an
 *  error-code: 0 means no error.
 *
 */
struct MatrixFns {

  /** Return a string containing name of implementing class.
   *  Set *err to EINVAL if this matrix is not in a valid state.  This
   *  function allows the use of reflective code which can take
   *  action based on the implementing class.
   */
  const char *(*getKlass)(const Matrix *this, int *err);

  /** Free all resources used by this. Set *err to EINVAL if this
   *  matrix is not in a valid state.
   */
  void (*free)(Matrix *this, int *err);

  /** Return # of rows of this matrix.  Set *err to EINVAL if this matrix
   *  not in valid state.
   */
  int (*getNRows)(const Matrix *this, int *err);

  /** Return # of columns of matrix. Set *err to EINVAL if this matrix
   *  not in valid state.
   */
  int (*getNCols)(const Matrix *this, int *err);

  /** Return element of this matrix entry at row rowIndex, col colIndex.
   *  Set *err to EINVAL if this matrix not in valid state; EDOM if rowIndex
   *  or colIndex not valid for this matrix.
   */
  MatrixBaseType (*getElement)(const Matrix *this,
                               int rowIndex, int colIndex, int *err);

  /** Set entry of this matrix entry at row rowIndex, col colIndex to
   *  element. Set *err to EINVAL if this matrix not in valid state; EDOM
   *  if rowIndex or colIndex not valid for this matrix.

   */
  void (*setElement)(Matrix *this, int rowIndex, int colIndex,
                     MatrixBaseType element, int *err);

  /** Set result matrix to transpose of this matrix.  Set *err to EINVAL
   *  if this or result matrix not in valid state; EDOM if dimensions
   *  of this and result are not compatible.
   */
  void (*transpose)(const Matrix *this, Matrix *result, int *err);

  /** Set product matrix to result of multiplying this matrix by
   *  multiplier matrix.  Before ths call, product should be a valid
   *  matrix; it's entries will be changed to contain the product
   *  matrix.  Set *err to EINVAL if this, multiplier or product matrix
   *  not in valid state; EDOM if dimensions of this, multiplier and product
   *  not compatible.
   */
  void (*mul)(const Matrix *this, const Matrix *multiplier,
              Matrix *product, int *err);

};

#endif //ifndef _MATRIX_H_
