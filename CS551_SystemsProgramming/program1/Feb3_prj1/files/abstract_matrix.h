#ifndef _ABSTRACT_MATRIX_H
#define _ABSTRACT_MATRIX_H

#include "matrix.h"


/** Return implementation of functions for an abstract matrix; these
 *  functions can be used by sub-classes to inherit behavior from this
 *  class. These are functions which can be implemented using only
 *  other matrix functions, independent of the actual implementation
 *  of the matrix.
 */
const MatrixFns *getAbstractMatrixFns(void);

#endif //ifndef _ABSTRACT_MATRIX_H
