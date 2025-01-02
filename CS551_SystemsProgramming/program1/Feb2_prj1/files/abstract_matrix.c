#include "abstract_matrix.h"

#include <errno.h>
#include <stdlib.h>

//TODO: Add types, data and functions as required.

static void freeMatrix( Matrix *pMatrix, int *pErr){
	*pErr = ( ((pMatrix->fns->getNRows(pMatrix, pErr)  > 0) && (pMatrix->fns->getNCols(pMatrix, pErr)>0))? 0 : EINVAL);
    free(pMatrix);
}

/* 
 * ---------- CONSTANT DEFINITION ----------
 */
static struct MatrixFns abstractFns = {
	.free = freeMatrix,
};

/** Return implementation of functions for an abstract matrix; these are
 *  functions which can be implemented using only other matrix functions,
 *  independent of the actual implementation of the matrix.
 */
const MatrixFns *
getAbstractMatrixFns(void)
{
  return &abstractFns;
}
