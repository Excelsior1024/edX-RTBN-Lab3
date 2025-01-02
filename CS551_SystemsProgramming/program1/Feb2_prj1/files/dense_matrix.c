#include "abstract_matrix.h"
#include "dense_matrix.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//TODO: Add types, data and functions as required.

/* 
 * ---------- STRUCTURE DEFINITION ----------
 */
struct DenseImplMatrixStruct {
	Matrix;						// anonymous field using -fms-extensions
	int	   			nRows;
	int    		    nCols;
	MatrixBaseType *pData[];	// flexi-array
};

/* 
 * ---------- PRIVATE FUNCTION DEFINITIONS ----------
 */
inline static checkMatrixState( const  struct DenseImplMatrixStruct *pConstDenseMatrix, int *pErr ){
	*pErr = ( ( (pConstDenseMatrix->nRows > 0) && (pConstDenseMatrix->nCols > 0 ) ) ? 0 : EINVAL );
}

inline static void checkPassedRowAndCol( const struct DenseImplMatrixStruct *pConstDenseMatrix, int passedRow, int passedCol, int *pErr ){
	*pErr = ( ( (passedRow > pConstDenseMatrix->nRows) || (passedCol > pConstDenseMatrix->nCols) ) ? EDOM : 0 );
	if(*pErr == EDOM ){		/* FIX ME */
		fprintf(stderr, "checkPassedRowAndCol  DEBUG");
	}
}

inline static void verifyCompatibleTransposeMatrix( const struct DenseImplMatrixStruct *pSourceMatrix, const struct DenseImplMatrixStruct *pDestMatrix, int *pErr){
	if ( (pSourceMatrix->nCols != pDestMatrix->nRows) && (pSourceMatrix->nRows != pDestMatrix->nCols)){
		*pErr = EDOM;
	}
}

inline static void checkMulCompatibility( const struct DenseImplMatrixStruct *pSrcMatrix, 
										  const struct DenseImplMatrixStruct *pMultiplierMatrix, 
										  struct DenseImplMatrixStruct *pProductMatrix, 
										  int *pErr){
											  
	if (pSrcMatrix->nRows != pProductMatrix->nRows){
		*pErr = EDOM;
	}
	else if( pSrcMatrix->nCols != pProductMatrix->nCols){
		*pErr = EDOM;
	}
	else if( pSrcMatrix->nCols != pMultiplierMatrix->nRows){
		*pErr = EDOM;
	}
	else if( pMultiplierMatrix->nCols != pProductMatrix->nCols){
		*pErr = EDOM;
	}											  
}

/* 
 * ----------  DENSE MATRIX FUNCTION DEFINITIONS ----------
 */
static const char * getKlass( const Matrix *pConstMatrix , int *pErr){
	const struct DenseImplMatrixStruct  *pConstDenseMatrix = (const struct DenseImplMatrixStruct *) pConstMatrix;
	checkMatrixState( pConstDenseMatrix, pErr );
	return "denseMatrix";
}

static int getNRows( const Matrix *pConstMatrix, int *pErr ){
	const struct DenseImplMatrixStruct  *pConstDenseMatrix = (const struct DenseImplMatrixStruct *) pConstMatrix;
	checkMatrixState( pConstDenseMatrix, pErr );
    return ( pConstDenseMatrix->nRows);
}

static int getNCols( const Matrix *pConstMatrix, int *pErr ){
	const struct DenseImplMatrixStruct  *pConstDenseMatrix = (const struct DenseImplMatrixStruct *) pConstMatrix;
	checkMatrixState( pConstDenseMatrix, pErr );
    return ( pConstDenseMatrix->nCols);	
}

static MatrixBaseType getElement( const Matrix *pConstMatrix, int rowIndex, int colIndex,  int *pErr){
	const struct DenseImplMatrixStruct  *pConstDenseMatrix = (const struct DenseImplMatrixStruct *) pConstMatrix;
	checkMatrixState( pConstDenseMatrix, pErr );
	checkPassedRowAndCol( pConstDenseMatrix, rowIndex, colIndex, pErr);
	return ( (MatrixBaseType) (pConstDenseMatrix->pData[ rowIndex * pConstDenseMatrix->nCols + colIndex]));
}

static void setElement( Matrix *pMatrix, int rowIndex, int colIndex, MatrixBaseType element, int *pErr){
	struct DenseImplMatrixStruct *pConstDenseMatrix = (struct DenseImplMatrixStruct *) pMatrix;
	checkMatrixState( pConstDenseMatrix, pErr );
	checkPassedRowAndCol( (const) pMatrix, rowIndex, colIndex, pErr);
	pConstDenseMatrix->pData[ rowIndex * pConstDenseMatrix->nCols + colIndex] = element;
}

// Assumes that the calling function has already allocated correctly sized memory for the transposed matrix
// Based on algorithm posted at:
// http://stackoverflow.com/questions/16737298/what-is-the-fastest-way-to-transpose-a-matrix-in-c
static void transpose( const Matrix *pMatrix, Matrix *pResult, int *pErr){
	const struct DenseImplMatrixStruct *pSrcMatrix = (const struct DenseImplMatrixStruct *) pMatrix;
	struct DenseImplMatrixStruct *pDestMatrix = (struct DenseImplMatrixStruct *) pResult;
	checkMatrixState( pSrcMatrix, pErr);  // Check that the rows/cols are non-zero
	checkMatrixState( pDestMatrix, pErr);
	verifyCompatibleTransposeMatrix( pSrcMatrix, pDestMatrix, pErr );
	if( !*pErr ){
		int N = pSrcMatrix->nRows;
		int M = pSrcMatrix->nCols;
		for( int n; n < N*M; n++ ){
			int i = n/N;			/* Integer division */
			int j = n%N;
			pDestMatrix->pData[n] = pSrcMatrix->pData[M*j + i];
		}
	}
}

// "Dumb" matrix multiplication:
// https://en.wikipedia.org/wiki/Matrix_multiplication_algorithm
static void mul( const Matrix *pMatrix, const Matrix *pMultiplier, Matrix *pProduct, int *pErr){
	const struct DenseImplMatrixStruct *pSrcMatrix = (const struct DenseImplMatrixStruct *) pMatrix;
	const struct DenseImplMatrixStruct *pMultiplierMatrix = (const struct DenseImplMatrixStruct *) pMultiplier;
	struct DenseImplMatrixStruct *pProductMatrix = (struct DenseImplMatrixStruct *) pProduct;

	checkMatrixState( pSrcMatrix, pErr);  // Check that the rows/cols are non-zero
	checkMatrixState( pMultiplierMatrix, pErr);  // Check that the rows/cols are non-zero
	checkMulCompatibility( pSrcMatrix, pMultiplierMatrix, pProductMatrix, pErr);
	
	if(!*pErr){
		int n = pSrcMatrix->nRows;
		int p = pMultiplierMatrix->nCols;
		int m = pSrcMatrix->nCols;
		int sum = 0;
		for(int i = 0; i < n; i++){
			for(int j = 0; j < p; j++){
				sum = 0;
				for(int k = 0; k < m; k++){
					sum += ((MatrixBaseType)(pSrcMatrix->pData[i*pSrcMatrix->nCols + k]) * (MatrixBaseType)(pMultiplierMatrix->pData[k*pMultiplierMatrix->nCols + j]));
				}
				pProductMatrix->pData[i*pProductMatrix->nCols + j] = sum;
			}
		}
	}
	
}


/* 
 * ---------- CONSTANT DEFINITION ----------
 */
static DenseMatrixFns denseMatrixFns = {
	.getKlass = getKlass,
	.getNRows = getNRows,
	.getNCols = getNCols,
	.getElement = getElement,
	.setElement = setElement,
	.transpose = transpose,
	.mul = mul,
};

/* 
 * ---------- PRIVATE GLOBAL VARIABLE(S) ----------
 */
static _Bool isInit = false;

/* 
 * --------------------------------- PUBLIC FUNCTIONS ---------------------------------
 */

/*
 * ---------------------------------------------------------------------------
 *  newDenseMatrix
 * --------------------------------------------------------------------------- 
 *  Return a newly allocated matrix with all entries in consecutive
 *  memory locations (row-major layout).  All entries in the newly
 *  created matrix are initialized to 0.  Set *err to EINVAL if nRows
 *  or nCols <= 0, to ENOMEM if not enough memory.
 * ---------------------------------------------------------------------------
 */
DenseMatrix *
newDenseMatrix(int nRows, int nCols, int *pErr)
{

  // Get memory from heap for the DenseImplMatrixStruct and the data
  int numElements = nRows * nCols;
  struct DenseImplMatrixStruct *pDenseMatrix = calloc( 1, ( sizeof(struct DenseImplMatrixStruct) + numElements*sizeof(MatrixBaseType)));
  if( !pDenseMatrix ){
	  *pErr = ENOMEM;
	  fprintf(stderr, "newDenseMatrix: DenseImplMatrixStruct memory allocation failure.\n");
	  exit(1);
  }
  pDenseMatrix->nRows = nRows;
  pDenseMatrix->nCols = nCols;
  
  // Complete the initialization of denseMatrixFns static structure.  
  // This only needs to be done once per program life-time.
  if( !isInit ){
	  const struct MatrixFns *fns = getAbstractMatrixFns();
	  denseMatrixFns.free = fns->free;
	  isInit = true;
  }
  
  pDenseMatrix->fns = (MatrixFns *)&denseMatrixFns;
  
  return ( (struct DenseMatrix *) pDenseMatrix);
}

/*
 * ---------------------------------------------------------------------------
 *  getDenseMatrixFns
 * --------------------------------------------------------------------------- 
 * Return implementation of functions for a dense matrix; these functions
 * can be used by sub-classes to inherit behavior from this class.
 * --------------------------------------------------------------------------- 
 */
const DenseMatrixFns *
getDenseMatrixFns(void)
{
  return &denseMatrixFns;
}
