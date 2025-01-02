#include "matrix_mul.h"

#include "errors.h"

//#define DO_TRACE 1
#include "trace.h"

#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <math.h>

#define 	READ	(0)
#define		WRITE	(1)
#define		DELIMITER	"\n"
#define		ERROR	(-1)
#define 	SUCCESS (0)
#define 	DEBUG 	(1)
#define		CHILD	(0)
#define		MAX_STRING_SIZE	(100)

typedef int filedes_TYPE[2];

struct MatrixMul {
	int			 nWorkers;			// number of worker processes
	FILE* 		 traceFile; 		// usually stderr
	pid_t*		 pChildPIDArray;	// array to keep child PIDs (one per worker)
	int*		 pPidStatusArray;	// array to keep exit status of child processes (one per worker)
	filedes_TYPE *pInPipeFDArray;	// inPipe Array - one per worker
	filedes_TYPE *pOutPipeFDArray;	// outPipe Array - one per worker
};

int test_malloc_ptr( void * ptr, char * errString, int *pErr ){
	
	if (ptr==NULL){
		*pErr = ENOMEM;
		fprintf(stderr,"%s [%s]\n", errString, strerror(errno));
		return ERROR;
	}
	return SUCCESS;
}

void error_handler( char * errString, int *pErr){
    *pErr = errno;
	fprintf(stderr,"%s [%s]\n", errString, strerror(errno));
	exit(ERROR);

}

void doDotProduct( int numRowsForThisChild, int thisChildsIndex, int n1, int n2, int n3, 
				   MatrixBaseType *pM1_Rows, MatrixBaseType *pM2, MatrixBaseType *pM3_Rows)
{
	
	MatrixBaseType *pTemp1, *pTemp2, *pTemp3;
	int 			i, j, k;
	
	
	pTemp1 = pM1_Rows;				// pTemp1 always points to the current M1 row element
	pTemp2 = pM2;					// pTemp2 always points to the current M2 element
	pTemp3 = pM3_Rows;				// pTemp3 always points to the current M3 row element
	for( i = 0; i < numRowsForThisChild; i++){
		for( j = 0; j<n3; j++){
			*pTemp3 = 0;
			pTemp2 = pM2+j;
			for( k = 0; k < n2; k++ ){
				*pTemp3 += (*pTemp1)*(*pTemp2);
				printf("CHILD PID# %d:  %d * %d = %d\n", getpid(), *pTemp1, *pTemp2, *pTemp3);
				pTemp1++;			// jump to next element of M1 row
				pTemp2 += n3;		// jump to next M2 column
			}
			printf("CHILD PID# %d .... Computing dot product:  %d\n",getpid(), *pTemp3);
			pTemp1 = pM3_Rows;
			pTemp3++;
		}
		pTemp2 = pM3_Rows;
		fprintf(stderr,"CHILD PID# %d Dot Products : [", getpid());
		for( j=0; j < n3; j++){
			fprintf(stderr, "%d, ", *pTemp2++);
		}
		fprintf(stderr, "]\n");
	}	
}


void doChildsWork( const struct MatrixMul *pMM, int i, int *pErr){
	fprintf(stderr, "\nHi!  Child # %d Working hard!\n", i);
	
	char 			 errString[MAX_STRING_SIZE];
	int 			 n1;							// number of rows in M1 and M3
	int				 n2;							// number of columns in M1 & number of rows in M2
	int				 n3;							// number of columns in M2 &  M3
	int				 j,k;
	MatrixBaseType   *pHead, *pM1_Rows, *pTemp1, *pM2, *pDotProducts;
	errno = 0;
	
	// This is the child process so close off the ends of the pipe we don't need
	if( close(pMM->pInPipeFDArray[i][WRITE]) <= ERROR ){
		sprintf(errString, "CHILD PID %d - pMM->pInPipeFDArray[%d][READ] CLOSE Error.", getpid(), i);
		error_handler(errString, pErr);			
	}
	if( close(pMM->pOutPipeFDArray[i][READ]) <= ERROR ){
		sprintf(errString, "CHILD PID %d - pMM->pInPipeFDArray[%d][READ] CLOSE Error.", getpid(), i);
		error_handler(errString, pErr);			
	}
	
	// Child i reads from inPipe[i][READ]
	if( read( pMM->pInPipeFDArray[i][READ], &n1, sizeof(int) ) <= ERROR ){
		sprintf(errString, "CHILD %d PID %d - pMM->pInPipeFDArray n1 read error.", i, getpid());
		error_handler(errString, pErr);					
	}
	if( read( pMM->pInPipeFDArray[i][READ], &n2, sizeof(int) ) <= ERROR ){
		sprintf(errString, "CHILD %d PID %d - pMM->pInPipeFDArray n2 read error.", i, getpid());
		error_handler(errString, pErr);					
	}
	if( read( pMM->pInPipeFDArray[i][READ], &n3, sizeof(int) ) <= ERROR ){
		sprintf(errString, "CHILD %d PID %d - pMM->pInPipeFDArray n3 read error.", i, getpid());
		error_handler(errString, pErr);					
	}
	fprintf(stderr,"\nChild # %d PID # %d:  Read n1 = %d  and n2 = %d and n3 = %d \n",i, getpid(), n1, n2, n3);

	pM2 = malloc( sizeof(MatrixBaseType) * n2 * n3 );
	sprintf(errString, "Child PID # %d: pM2 malloc error ", getpid()); 
	if( test_malloc_ptr(pM2, errString, pErr ) == SUCCESS ){
		pTemp1 = pM2;
		// Read in all of M2
		for ( j = 0; j < n2; j++ ){
			for( k = 0; k < n3; k++ ) {
				if( read( pMM->pInPipeFDArray[i][READ], pTemp1, sizeof(MatrixBaseType) ) <= ERROR ){
					sprintf(errString, "CHILD %d PID %d - Error reading M2[%d][%d]", i, getpid(), j, k);
					goto DOCHILDSWORK_ERROR_LABEL01;									
				}
				printf("CHILD PID# %d Reading b[%d][%d] = %d \n", getpid(), j, k, *pTemp1);
				pTemp1++;
			}
		}
	}
	else{ goto DOCHILDSWORK_ERROR_LABEL00; }
	
	int minRowsPerChild = (int) floor( (double) n1/ (double) pMM->nWorkers);	
	int numRowsForThisChild = ( ((minRowsPerChild*pMM->nWorkers)+i) < n1 ?  (minRowsPerChild*pMM->nWorkers)+i : minRowsPerChild );
	pM1_Rows = malloc( sizeof(MatrixBaseType) * numRowsForThisChild * n2 );
	sprintf(errString, "Child PID # %d: pM1_Rows malloc error  (numRowsForThisChild = %d) ", getpid(), numRowsForThisChild); 
	if( test_malloc_ptr(pM1_Rows, errString, pErr ) == SUCCESS ){
		pTemp1 = pM2;
		// Read in all assigned M1 rows
		for ( j = 0; j < numRowsForThisChild; j++ ){
			for( k = 0; k < n2; k++ ) {
				if( read( pMM->pInPipeFDArray[i][READ], pTemp1, sizeof(MatrixBaseType) ) <= ERROR ){
					sprintf(errString, "CHILD %d PID %d - Error reading M1 Row[%d][%d]", i, getpid(), j, k);
					goto DOCHILDSWORK_ERROR_LABEL02;										
				}
				printf("CHILD PID# %d Reading M1 Row[%d][%d] = %d \n", getpid(), j, k, *pTemp1);
				pTemp1++;
			}
		}
	}
	else{ goto DOCHILDSWORK_ERROR_LABEL01; }
	
	// Got all of the data for this child, so get some memory and calculate the dot products
	pDotProducts = malloc( sizeof(MatrixBaseType) * numRowsForThisChild * n3 );
	sprintf(errString, "Child PID # %d: pDotProducts malloc error ", getpid(), numRowsForThisChild); 
	if( test_malloc_ptr( pDotProducts, errString, pErr) == SUCCESS ){
	
		doDotProduct( numRowsForThisChild, i, n1, n2, n3, pM1_Rows, pM2, pDotProducts);
		
		// Now write the dot products to the write outPipe
		pTemp1 = pDotProducts;
		for( j=0; j<(numRowsForThisChild*n2); j++){
			if( write( pMM->pOutPipeFDArray[i][WRITE], pTemp1, sizeof(MatrixBaseType) ) <= ERROR ){
				sprintf(errString, "CHILD %d PID %d - Error writing dot-product %d to outPipe[%d]", i, getpid(), *pTemp1, i);
				goto DOCHILDSWORK_ERROR_LABEL02;										
			}
			printf("CHILD # %d PID# %d Writing DotProduct = %d to OutPipe[%d] \n", i, getpid(), *pTemp1, i);
			pTemp1++;
		}
	}
	else { goto DOCHILDSWORK_ERROR_LABEL02; }
	
	free(pDotProducts);
	free(pM1_Rows);
	free(pM2);	
	return;		// The no error return path
	
// Error handling for the mallocs	
DOCHILDSWORK_ERROR_LABEL02:	
	free(pM1_Rows);
DOCHILDSWORK_ERROR_LABEL01:	
	free(pM2);
DOCHILDSWORK_ERROR_LABEL00:	
	error_handler(errString, pErr);
}

/** Return a multi-process matrix multiplier with nWorkers worker
 *  processes.  Set *err to an appropriate error number (documented in
 *  errno(3)) on error.
 *
 *  If trace is not NULL, turn on tracing for calls to mulMatrixMul()
 *  using returned MatrixMul.  Specifically, each dot-product
 *  computation must be logged to trace as a line in the form:
 *
 *  INDEX[PID]: [I]x[J] = Z
 *
 *  where INDEX in [0, nWorkers) is the index of the worker and PID is
 *  its pid, I is the index of the row in the multiplicand, J is the
 *  index of the row in the multiplier and Z is their dot-product
 *  computed by child process PID.  The spacing must be exactly as
 *  shown above and all values must be output in decimal with no
 *  leading zeros or redundant + signs.
 *
 */
MatrixMul *
newMatrixMul(int nWorkers, FILE *trace, int *err)
{
  printf("*** PID# %d Invoking newMatrixMul.\n", getpid() );
  MatrixMul *pMatrixMulObj = NULL;
  int 		i, j, k;
  
  // Check the nWorkers parameter
  if( nWorkers >= 1 ) 
  {  
    pMatrixMulObj = malloc( sizeof(MatrixMul) );
	if( test_malloc_ptr(pMatrixMulObj,"newMatrixMul:pMatrixMulObj malloc error", err) == SUCCESS )
	{
		pMatrixMulObj->nWorkers = nWorkers; 
		pMatrixMulObj->traceFile = trace;
		
		pMatrixMulObj->pChildPIDArray = malloc( nWorkers*(sizeof(pid_t)));
		if( test_malloc_ptr(pMatrixMulObj->pChildPIDArray,"newMatrixMul:pMatrixMulObj malloc error", err) == ERROR) {
			goto  NEWMATRIXMUL_ERROR_LABEL01; 
		}
		
		pMatrixMulObj->pPidStatusArray = malloc( nWorkers*(sizeof(int)));
		if( test_malloc_ptr( pMatrixMulObj->pPidStatusArray, "newMatrixMul: pMatrixMulObj->pPidStatusArray malloc error", err ) == ERROR ) {
			goto  NEWMATRIXMUL_ERROR_LABEL02; 
		}
		
        // Allocate memory for the pipes - one inPipe and one outPipe per worker
        pMatrixMulObj->pInPipeFDArray = malloc( sizeof(filedes_TYPE) * nWorkers );
		if( test_malloc_ptr( pMatrixMulObj->pInPipeFDArray, "newMatrixMul: pMatrixMulObj->pInPipeFDArray malloc error", err ) == ERROR ) {
			goto  NEWMATRIXMUL_ERROR_LABEL03; 
		}
		
        pMatrixMulObj->pOutPipeFDArray = malloc( sizeof(filedes_TYPE) * nWorkers );
		if( test_malloc_ptr( pMatrixMulObj->pOutPipeFDArray, "newMatrixMul: pMatrixMulObj->pOutPipeFDArray malloc error", err ) == ERROR ) {
			goto  NEWMATRIXMUL_ERROR_LABEL04; 
		}
		
		// Tell the kernel to create the pipes
		errno = 0;
		for( i=0; i<nWorkers; i++){
			if ( (pipe( pMatrixMulObj->pInPipeFDArray[i]) < 0) || (pipe( pMatrixMulObj->pOutPipeFDArray[i]) < 0) ){
			  *err = errno;
			  fprintf(stderr, "Pipe Error:  %s\n", strerror(errno));
			  goto NEWMATRIXMUL_ERROR_LABEL05;
			}
			
#if 1		
			// DEBUG
			struct stat st;
			errno = 0;
			if( fstat( pMatrixMulObj->pInPipeFDArray[i][READ], &st )){ printf("\nfstat error  pMatrixMulObj->pInPipeFDArray[%d][READ]:  [%s]\n", i, strerror(errno)); 	} 
			else if(S_ISFIFO(st.st_mode)){ 	printf("File descriptor  pMatrixMulObj->pInPipeFDArray[%d][READ] is a FIFO.\n", i);	}
			else{ printf(" pMatrixMulObj->pInPipeFDArray[%d][READ]: ERROR - this is not a pipe!!! \n", i);  goto 	NEWMATRIXMUL_ERROR_LABEL05; }
			if( fstat( pMatrixMulObj->pInPipeFDArray[i][WRITE], &st )){ printf("\nfstat error  pMatrixMulObj->pInPipeFDArray[%d][WRITE]:  [%s]\n", i, strerror(errno)); 	} 
			else if(S_ISFIFO(st.st_mode)){ 	printf("File descriptor  pMatrixMulObj->pInPipeFDArray[%d][WRITE] is a FIFO.\n", i);	}
			else{ printf(" pMatrixMulObj->pInPipeFDArray[%d][WRITE]: ERROR - this is not a pipe!!! \n", i);  goto 	NEWMATRIXMUL_ERROR_LABEL05; }
			
			if( fstat( pMatrixMulObj->pOutPipeFDArray[i][READ], &st )){ printf("\nfstat error  pMatrixMulObj->pOutPipeFDArray[%d][READ]:  [%s]\n", i, strerror(errno)); 	} 
			else if(S_ISFIFO(st.st_mode)){ 	printf("File descriptor  pMatrixMulObj->pOutPipeFDArray[%d][READ] is a FIFO.\n", i);	}
			else{ printf(" pMatrixMulObj->pOutPipeFDArray[%d][READ]: ERROR - this is not a pipe!!!!!! \n",i); goto NEWMATRIXMUL_ERROR_LABEL05;	}
			if( fstat( pMatrixMulObj->pOutPipeFDArray[i][WRITE], &st )){ printf("\nfstat error  pMatrixMulObj->pOutPipeFDArray[i][WRITE]:  [%s]\n", strerror(errno)); 	} 
			else if(S_ISFIFO(st.st_mode)){ 	printf("File descriptor  pMatrixMulObj->pOutPipeFDArray[%d][WRITE] is a FIFO.\n", i);}
			else{ printf(" pMatrixMulObj->pOutPipeFDArray[%d][WRITE]: ERROR - this is not a pipe!!!!!! \n", i);  goto NEWMATRIXMUL_ERROR_LABEL05;}
#endif		
		}
		// Create the processes
		pid_t *pPid = pMatrixMulObj->pChildPIDArray;
		for( i=0; i<nWorkers; i++){
			errno = 0;
			*pPid = fork();
			switch( *pPid ){
				case(ERROR):
					*err = errno;
					fprintf(stderr, "Fork %d Error:  %s\n", i, strerror(errno) );
					break;
				case(CHILD):
					fprintf(stderr, "*** CHILD PID# %d - fork(): fork() returned PID %d  (myParentPID # %d) \n", getpid(), (int) *pPid, getppid());
					doChildsWork(pMatrixMulObj, i, err);
					break;
				default:   /* PARENT */
					fprintf(stderr, "*** PARENT PID# %d - fork(): fork() returned PID %d  (myParentPID # %d) \n", getpid(), (int) *pPid, getppid());
					break;
			}
			pPid++;
		}
		
		return pMatrixMulObj;		
	}
	else { goto NEWMATRIXMUL_ERROR_LABEL00; }
  }
  else {
	  *err = EINVAL;
	   fprintf(stderr,"newMatrixMul:  incorrect value supplied for nWorkers [%s]", strerror(errno));
	   return NULL;
  }
/* Error handling for the various malloc() calls */
NEWMATRIXMUL_ERROR_LABEL05:
	free(pMatrixMulObj->pOutPipeFDArray);
	
NEWMATRIXMUL_ERROR_LABEL04:
	free(pMatrixMulObj->pInPipeFDArray);
  
NEWMATRIXMUL_ERROR_LABEL03:
	free(pMatrixMulObj->pPidStatusArray);

NEWMATRIXMUL_ERROR_LABEL02:
	free(pMatrixMulObj->pChildPIDArray);
  
NEWMATRIXMUL_ERROR_LABEL01:
	free(pMatrixMulObj);
		
NEWMATRIXMUL_ERROR_LABEL00:	
	return NULL;
 
}

/** Free all resources used by matMul.  Specifically, free all memory
 *  and return only after all child processes have been set up to
 *  exit.  Set *err appropriately (as documented in errno(3)) on error.
 */
void
freeMatrixMul(MatrixMul *matMul, int *err)
{
  printf("*** Invoking freeMatrixMul.\n");
  free( matMul );
  return;
}

/** Set matrix c[n1][n3] to a[n1][n2] * b[n2][n3].  It is assumed that
 *  the caller has allocated c[][] appropriately.  Set *err to an
 *  appropriate error number (documented in errno(3)) on error.  If
 *  *err is returned as non-zero, then the matMul object may no longer
 *  be valid and future calls to mulMatrixMul() may have unpredictable
 *  behavior.  It is the responsibility of the caller to call
 *  freeMatrixMul() after an error.
 *
 *  All dot-products of rows from a[][] and columns from b[][] must be
 *  performed using the worker processes which were already created in
 *  newMatrixMul() and all IPC must be handled using anonymous pipes.
 *  The multiplication should be set up in such a way so as to allow
 *  the worker processes to work on different dot-products
 *  concurrently.
 */
void
mulMatrixMul(const MatrixMul *matMul, int n1, int n2, int n3,
             CONST MatrixBaseType a[n1][n2], 	// aka M1
             CONST MatrixBaseType b[n2][n3],	// aka M2
             MatrixBaseType c[n1][n3], int *err)
{
    printf("\n*** PID# %d Invoking mulMatrixMul.\n", getpid() );
    int i, j, k;
    char 	errString[MAX_STRING_SIZE];
	// This is the parent process so close off the ends of the pipe we don't need
	for(i=0; i< matMul->nWorkers; i++){
		errno = 0;
		if( close(matMul->pInPipeFDArray[i][READ]) <= ERROR ){
			sprintf(errString, "matMul->pInPipeFDArray[%d][READ] CLOSE Error.", i);
			error_handler(errString, err);			
		}
		if( close(matMul->pOutPipeFDArray[i][WRITE]) <= ERROR ){
			sprintf(errString, "matMul->pInPipeFDArray[%d][WRITE] CLOSE Error.", i);
			error_handler(errString, err);			
		}
	}
  
	// Write the input matrix data to each inPipe.
	// Each inPipe will contain first the nRow and nColumns for the second (b) matrix (M2).
	// Then, all the data that comprises M2 (all rows and columns)
	// Then the row data for each of the rows of M1 that this process is responsible for.
	// Note that the number of row data for each M1 row equals nColums for M2
	for(i=0; i< n1; i++){
		
		// If this is the first time we've written to this pipe, we need to write M2
		if(i<matMul->nWorkers){
			// http://stackoverflow.com/questions/5237041/how-to-send-integer-with-pipe-between-two-processes
			// Write M2 row
			errno=0;
			if( write( matMul->pInPipeFDArray[i][WRITE], &(n1), sizeof( int ) ) <= ERROR ){  
				sprintf(errString, "PID %d mulMatrixMul: pInPipeFDArray[%d][WRITE] n1 Write Error", getpid(), i);
				error_handler(errString, err);
			}
			printf("PARENT PID# %d WRITING: n1 = %d\n", getpid(), n1);
			if( write( matMul->pInPipeFDArray[i][WRITE], &(n2), sizeof( int ) ) <= ERROR ){  
				sprintf(errString, "PID %d mulMatrixMul: pInPipeFDArray[%d][WRITE] n2 Write Error", getpid(), i);
				error_handler(errString, err);
			}
			printf("PARENT PID# %d WRITING: n2 = %d\n", getpid(), n2);
			// Write M2 column
			if( write( matMul->pInPipeFDArray[i][WRITE], &(n3), sizeof( int ) ) <= ERROR ){  
				sprintf(errString, "PID %d mulMatrixMul: pInPipeFDArray[%d][WRITE] n3 Write Error", getpid(), i);
			}
			printf("PARENT PID# %d WRITING: n3 = %d\n", getpid(), n3);
			// Write the M2 matrix
			for ( j = 0; j < n2; j++ ){
				for( k = 0; k < n3; k++ ) {
					if( write( matMul->pInPipeFDArray[i][WRITE], &(b[j][k]), sizeof( b[j][k]) ) <= ERROR ){
						sprintf(errString, "mulMatrixMul: pInPipeFDArray[%d][WRITE] b[%d][%d] Write Error", i, j, k);
						error_handler(errString, err);
					}
					printf("PARENT PID# %d WRITING M2 to InPipe[%d]:  b[%d][%d] = %d\n",getpid(), i,  j, k, b[j][k]);
				}
			}
		}
		int ceiling = (int) ceil( ( (double) n1 / (double) matMul->nWorkers) );
		// Now write all applicable M1 rows to this pipe
		for( j=0; j<ceiling; j++){
			int rowIndex = matMul->nWorkers * j + i;
			if( rowIndex < n1 ){
				for ( k = 0; k < n2; k++ ){
					if( write( matMul->pInPipeFDArray[i][WRITE], &(a[rowIndex][k]), sizeof( a[rowIndex][k]) ) <= ERROR ){
						sprintf(errString, "mulMatrixMul: pInPipeFDArray[%d][WRITE] a[%d][%d] Write Error", i, rowIndex, k);
						error_handler(errString, err);						
					}
					printf("PARENT PID# %d WRITING  ROW %d of M1 to InPipe[%d]::  a[%d][%d] = %d\n",getpid(), rowIndex, i, rowIndex, k, b[rowIndex][k]);
				}
			}
		}
		
	}

}
