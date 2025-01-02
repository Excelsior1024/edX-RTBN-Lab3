#include "dense_matrix.h"
#include "smart_mul_matrix.h"
#include "abstract_matrix.h"

#include <errno.h>
#include <stdbool.h>

//TODO: Add types, data and functions as required.

// Forward reference
static SmartMulMatrixFns smartMatrixFns;

/* 
 * ---------- STRUCTURE DEFINITION ----------
 */
struct SmartImplMatrixStruct {
	Matrix;						// anonymous field using -fms-extensions
	int	   			nRows;
	int    		    nCols;
	MatrixBaseType  pData[];	// flexi-array 
};

/* 
 * ---------- PRIVATE FUNCTION DEFINITIONS ----------
 */
inline static void checkMatrixState( const  struct SmartImplMatrixStruct *pConstDenseMatrix, int *pErr ){
	*pErr = ( ( (pConstDenseMatrix->nRows > 0) && (pConstDenseMatrix->nCols > 0 ) ) ? 0 : EINVAL );
}

inline static void checkPassedRowAndCol( const struct SmartImplMatrixStruct *pConstDenseMatrix, int passedRow, int passedCol, int *pErr ){
	*pErr = ( ( (passedRow > pConstDenseMatrix->nRows) || (passedCol > pConstDenseMatrix->nCols) ) ? EDOM : 0 );
}

inline static void verifyCompatibleTransposeMatrix( const struct SmartImplMatrixStruct *pSourceMatrix, const struct SmartImplMatrixStruct *pDestMatrix, int *pErr){
	if ( (pSourceMatrix->nCols != pDestMatrix->nRows) && (pSourceMatrix->nRows != pDestMatrix->nCols)){
		*pErr = EDOM;
	}
}

inline static void checkMulCompatibility( const struct SmartImplMatrixStruct *pSrcMatrix, 
										  const struct SmartImplMatrixStruct *pMultiplierMatrix, 
										  struct SmartImplMatrixStruct *pProductMatrix, 
										  int *pErr){
											  

	if ( (pSrcMatrix->nRows != pProductMatrix->nRows)        ||
	     (pMultiplierMatrix->nCols != pProductMatrix->nCols) || 
	     (pSrcMatrix->nCols != pMultiplierMatrix->nRows) ) {
			*pErr = EDOM;
	}
}


/* 
 * ----------  SMART MATRIX FUNCTION DEFINITIONS ----------
 */
static const char * getKlass( const Matrix *pConstMatrix , int *pErr){
	const struct SmartImplMatrixStruct  *pConstSmartMatrix = (const struct SmartImplMatrixStruct *) pConstMatrix;
	checkMatrixState( pConstSmartMatrix, pErr );
	return "smartMatrix";
}

// "Smart" matrix multiplication per project requirements
static void mul( const Matrix *pMatrix, const Matrix *pMultiplier, Matrix *pProduct, int *pErr){
	const struct SmartImplMatrixStruct *pSrcMatrix = (const struct SmartImplMatrixStruct *) pMatrix;
	const struct SmartImplMatrixStruct *pMultiplierMatrix = (const struct SmartImplMatrixStruct *) pMultiplier;
	struct SmartImplMatrixStruct *pProductMatrix = (struct SmartImplMatrixStruct *) pProduct;

	// pSrcMatrix:  Check that the rows/cols are non-zero
	checkMatrixState( pSrcMatrix, pErr);  	
	if (*pErr) goto SMART_MUL_ERROR_LABEL;
	
	// pMultiplierMatrix:  Check that the rows/cols are non-zero
	checkMatrixState( pMultiplierMatrix, pErr);  
	if (*pErr) goto SMART_MUL_ERROR_LABEL;
	
	// pProduct:  Check that the rows/cols are non-zero
	checkMatrixState( pProductMatrix, pErr);  
	if (*pErr) goto SMART_MUL_ERROR_LABEL;
	
	// Verify that the passed matrices are compatible for matrix multiplication
	checkMulCompatibility( pSrcMatrix, pMultiplierMatrix, pProductMatrix, pErr);
	if (*pErr) goto SMART_MUL_ERROR_LABEL;
	
	// Get the intermediate matrices used to get the smart-mul product	
	struct SmartImplMatrixStruct *pNewProduct = (struct SmartImplMatrixStruct *) newSmartMulMatrix(pProductMatrix->nCols, pProductMatrix->nRows, pErr);
	if (*pErr) goto SMART_MUL_ERROR_LABEL;		// don't need to free the intermediate matrices; they weren't created
	struct SmartImplMatrixStruct *pTransMultiplier = (struct SmartImplMatrixStruct *) newSmartMulMatrix(pMultiplierMatrix->nCols, pMultiplierMatrix->nRows, pErr);
	if (*pErr) goto SMART_MUL_ERROR_LABEL01;	// need to free the pNewProduct matrix

	// Transpose the Multiplier Matrix
	smartMatrixFns.transpose( (const Matrix *)pMultiplierMatrix, (Matrix *)pTransMultiplier, pErr);
	if (*pErr) goto SMART_MUL_ERROR_LABEL02;	// need to free both intermediate matrices.
	
	// Change the multiplication algorithm to go row x row (instead of row x column)
	int n = pSrcMatrix->nRows;
	int p = pTransMultiplier->nRows;
	int m = pSrcMatrix->nCols;
	int sum = 0;
	for(int i = 0; i < n; i++){
		for(int j = 0; j < p; j++){
			sum = 0;
			for(int k = 0; k < m; k++){
				sum += ((MatrixBaseType)(pSrcMatrix->pData[i*pSrcMatrix->nCols + k]) * (MatrixBaseType)(pMultiplierMatrix->pData[k*pMultiplierMatrix->nCols + j]));
			}
			pNewProduct->pData[i*pProductMatrix->nCols + j] = sum;
		}
	}
	// Transpose the pNewProduct back into the pProduct matrix
	smartMatrixFns.transpose( (const Matrix *)pNewProduct, (Matrix *)pProduct, pErr);
	if (*pErr) goto SMART_MUL_ERROR_LABEL02;	// need to free both intermediate matrices.
	
	/*
	 * Free the temporary matrices used to do the Smart Matrix Multiply  in the revers order that they were instantiated.
	 */
SMART_MUL_ERROR_LABEL02:
	smartMatrixFns.free((Matrix *) pTransMultiplier, pErr);
	
SMART_MUL_ERROR_LABEL01:
	smartMatrixFns.free((Matrix *) pNewProduct, pErr);
	
SMART_MUL_ERROR_LABEL:
	return;
}

/* 
 * ---------- PRIVATE GLOBAL VARIABLES ----------
 */
static SmartMulMatrixFns smartMatrixFns = {
	.getKlass = getKlass,
	.mul = mul,
};

static _Bool isInit = false;

/* 
 * --------------------------------- PUBLIC FUNCTIONS ---------------------------------
 */

/*
 * ---------------------------------------------------------------------------
 *  getSmartMulMatrixFns
 * --------------------------------------------------------------------------- 
 *  Return a newly allocated matrix with all entries in consecutive
 *  memory locations (row-major layout).  All entries in the newly
 *  created matrix are initialized to 0.  The return'd matrix uses
 *  a smart multiplication algorithm to avoid caching issues;
 *  specifically, transpose the multiplier and use a modified
 *  multiplication algorithm with the transposed multiplier.
 * --------------------------------------------------------------------------- 
 */
SmartMulMatrix *
newSmartMulMatrix(int nRows, int nCols, int *err)
{

  // Instantiate a SmartImplMatrixStruct object.  Let newDenseMatric handle all the 
  // error checking that verifies memory is correctly allocated.
  struct SmartImplMatrixStruct *pSmartImplMatrix = (struct SmartImplMatrixStruct *)newDenseMatrix(nRows, nCols, err);
  pSmartImplMatrix->nRows = nRows;
  pSmartImplMatrix->nCols = nCols;
  
  // Complete the initialization of smartMatrixFns static structure.  
  // This only needs to be done once per program life-time.
  if( !isInit ){
	  const MatrixFns *fns = getAbstractMatrixFns();
	  smartMatrixFns.free = fns->free;
	  const DenseMatrixFns * dfns = getDenseMatrixFns();
	  smartMatrixFns.getNRows = dfns->getNRows;
	  smartMatrixFns.getNCols = dfns->getNCols;
	  smartMatrixFns.getElement = dfns->getElement;
	  smartMatrixFns.setElement = dfns->setElement;
	  smartMatrixFns.transpose  = dfns->transpose;
	  isInit = true;
  }
  
  pSmartImplMatrix->fns = (MatrixFns *)&smartMatrixFns;
  
  return ((SmartMulMatrix *) pSmartImplMatrix);
}

/*
 * ---------------------------------------------------------------------------
 *  getSmartMulMatrixFns
 * --------------------------------------------------------------------------- 
 * Return implementation of functions for a smart multiplication
 * matrix; these functions can be used by sub-classes to inherit
 * behavior from this class.
 * --------------------------------------------------------------------------- 
 */
const SmartMulMatrixFns *
getSmartMulMatrixFns(void)
{
  return &smartMatrixFns;
}
