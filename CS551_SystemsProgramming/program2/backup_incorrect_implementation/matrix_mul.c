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

#define 	READ	(0)
#define		WRITE	(1)
#define		DELIMITER	"\n"
#define		ERROR	(-1)
#define 	SUCCESS (0)
#define 	DEBUG 	(1)
struct MatrixMul {
	int		nWorkers;			// number of worker processes
	FILE* 	traceFile; 			// usually stderr
	pid_t*	pChildPIDArray;		// array to keep child PIDs (one per worker)
	int*	pPidStatusArray;	// array to keep exit status of child processes (one per worker)
};


typedef int filedes_TYPE[2];

int test_malloc_ptr( void * ptr, char * errString, int *pErr ){
	
	if (ptr==NULL){
		*pErr = ENOMEM;
		fprintf(stderr,"%s [%s]\n", errString, strerror(errno));
		return ERROR;
	}
	return SUCCESS;
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
  printf("*** Invoking newMatrixMul.\n");
  MatrixMul *pMatrixMulObj = NULL;
  
  // Check the nWorkers parameter
  if( nWorkers >= 1 ) {  
    pMatrixMulObj = malloc( sizeof(MatrixMul) );
	if( test_malloc_ptr(pMatrixMulObj,"newMatrixMul:pMatrixMulObj malloc error", err) == SUCCESS ){
		pMatrixMulObj->nWorkers = nWorkers; 
		pMatrixMulObj->traceFile = trace;
		pMatrixMulObj->pChildPIDArray = malloc( nWorkers*(sizeof(pid_t)));
		test_malloc_ptr( pMatrixMulObj->pChildPIDArray, "newMatrixMul: pMatrixMulObj->pChildPIDArray malloc error", err );
		pMatrixMulObj->pPidStatusArray = malloc( nWorkers*(sizeof(int)));
		test_malloc_ptr( pMatrixMulObj->pPidStatusArray, "newMatrixMul: pMatrixMulObj->pPidStatusArray malloc error", err );
	}
  }
  else {
	  *err = EINVAL;
	  fprintf(stderr,"newMatrixMul:  incorrect value supplied for nWorkers [%s]", strerror(errno));
  }
  return pMatrixMulObj;
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
             CONST MatrixBaseType a[n1][n2],
             CONST MatrixBaseType b[n2][n3],
             MatrixBaseType c[n1][n3], int *err)
{
  // VARIABLES THAT WILL BE GLOBAL TO ALL PROCs (including the forked processes)
  int i, j, k, n, status;
  pid_t pid;
  char  myErrString[80];
  pid_t* pChildPidTemp = matMul->pChildPIDArray;
  
    // Check parameters for validity
  if ( matMul == NULL || n1 < 1 || n2 < 1 || n3 < 1 ) { *err = EINVAL; return;  }
  
  // Create the pipes
  filedes_TYPE *pInPipeFDArray = malloc( sizeof(filedes_TYPE) * n1 );
  filedes_TYPE *pOutPipeFDArray = malloc( sizeof(filedes_TYPE) * n1 );
  if( (pInPipeFDArray == NULL) || (pOutPipeFDArray == NULL) ){  *err = ENOMEM;  return; }
    
  for( i = 0; i < n1; i++){

	// Make the pipes
	if ( (pipe(pInPipeFDArray[i]) < 0) || (pipe(pOutPipeFDArray[i]) < 0) ){
	  *err = errno;
	  fprintf(stderr, "Pipe Error:  %s\n", strerror(errno));
	}
	
    struct stat st;
	errno = 0;
	if( fstat(pInPipeFDArray[i][READ], &st )){ printf("\nfstat error pInPipeFDArray[i][READ]:  [%s]\n", strerror(errno)); 	} 
	else if(S_ISFIFO(st.st_mode)){ 	printf("File descriptor pInPipeFDArray[i][READ] is a FIFO.\n");	}
	else{ printf("pInPipeFDArray[i][READ]: I have no idea what is going on!!! \n"); 	}
	  
	if( fstat(pOutPipeFDArray[i][WRITE], &st )){ printf("\nfstat error pOutPipeFDArray[i][WRITE]:  [%s]\n", strerror(errno)); 	} 
	else if(S_ISFIFO(st.st_mode)){ 	printf("File descriptor pOutPipeFDArray[i][WRITE] is a FIFO.\n");	}
	else{ printf("pOutPipeFDArray[i][WRITE]: I have no idea what is going on!!! \n"); 	}
  }
  
  // Fork the processes - use a process fan
  for( i = 0; i < matMul->nWorkers; i++ ){
	  pid = fork();
	  if( pid < 0 ){
		*err = errno;
		fprintf(stderr, "Fork Error:  %s\n", strerror(errno));
		break;
	  }
	  /**
	    * -----------------  CHILD PROCESSING -----------------
		*/
	  else if (pid == 0) {
 	    fprintf(stderr, "*** CHILD PID# %d - fork(): Returned PID %d  (myParentPID # %d) \n", getpid(), (int) pid, getppid());
		
		if( n1 < matMul->nWorkers){
			printf("CHILD PID# %d FEWER ROWS THAN WORKERS!  Number of M1 rows = %d   Number of workers = %d \n", getpid(), n2, matMul->nWorkers);
			
			if ( i < n1 ){
				// --------------------------------------------------------------------
				// FIRST, read in the matrix data from my in-pipe written by the parent
				// --------------------------------------------------------------------
				printf("CHILD PID# %d  I am worker # %d and this is row %d so I MUST WORK!\n", getpid(), i, i);
				close(pInPipeFDArray[i][WRITE]);
				close(pOutPipeFDArray[i][READ]);
				int *pHead, *pM1_Row, *pTemp1, *pM2, *pM3, *pTemp2, *pTemp3;	
				errno = 0;
				pHead = malloc( (sizeof(int)*n2) + (sizeof(int)*n2*n3) );
				sprintf(myErrString, "CHILD PID# %d - Malloc Error (pHead). ", getpid());
				if (test_malloc_ptr(pHead, myErrString, err) == SUCCESS) {
					pTemp1 = pHead;		
					// Read in the M1 row
					for( j = 0; j < n2; j++ ){
						n = read( pInPipeFDArray[i][READ], pTemp1, sizeof(int) );
						printf("CHILD PID# %d Reading a[%d][%d] = %d \n", getpid(), i, j, *pTemp1);
						pTemp1++;
					}
					// Read in all of M2
					for ( j = 0; j < n2; j++ ){
						for( k = 0; k < n3; k++ ) {
							n = read( pInPipeFDArray[i][READ], pTemp1, sizeof(int) );
							printf("CHILD PID# %d Reading b[%d][%d] = %d \n", getpid(), j, k, *pTemp1);
							pTemp1++;
						}
					}
					close(pInPipeFDArray[i][READ]);
					}
				else { break; }
				
				// --------------------------------------------------------------------
				// SECOND, we got the data.  Now compute dot products
				// --------------------------------------------------------------------
				pM1_Row = pHead;
				pTemp1 = pM1_Row;		// pTemp1 always points to the current  M1 row element
				pM2 = (pHead + n2);		// index to where M2 begins
				pTemp2 = pM2;			// pTemp2 always points to the current M2 element
				pM3 = malloc( n3*sizeof(int) );
				pTemp3 = pM3;			// pTemp3 always points to the current M3 row element
				sprintf(myErrString, "CHILD PID# %d - Malloc Error (pM3). ", getpid());
				if (test_malloc_ptr(pM3, myErrString, err) == SUCCESS) {
					// For each dot product in the M3 row
					for( j = 0; j < n3; j++){
						*pTemp3 = 0;
						pTemp2 = pM2+j;
						for( k = 0; k < n2; k++ ){
							*pTemp3 += (*pTemp1)*(*pTemp2);
							printf("CHILD PID# %d:  %d * %d = %d\n", getpid(), *pTemp1, *pTemp2, *pTemp3);
							pTemp1++;			// jump to next element of M1 row
							pTemp2 += n3;		// jump to next M2 column
						}
						printf("CHILD PID# %d .... Computing dot product:  %d\n",getpid(), *pTemp3);
						pTemp1 = pM1_Row;
						pTemp3++;
					}
					pTemp2 = pM3;
					fprintf(stderr,"CHILD PID# %d Dot Products : [", getpid());
					for( j=0; j < n3; j++){
						fprintf(stderr, "%d, ", *pTemp2++);
					}
					fprintf(stderr, "]\n");
				}	
				else { break; }

				// --------------------------------------------------------------------
				// THIRD, we got the dot products.  Now write them to the out-pipes
				// --------------------------------------------------------------------
				// http://stackoverflow.com/questions/5237041/how-to-send-integer-with-pipe-between-two-processes
				// Write the designated M1 row to the pipe
				pTemp3 = pM3;
				for ( j = 0; j < n3; j++ ){
					write( pOutPipeFDArray[i][WRITE], pTemp3, sizeof(int) );
					printf("CHILD PID# %d:  Write %d to outPipe\n", getpid(), *pTemp3);
					pTemp3++;
				}
				close(pOutPipeFDArray[i][WRITE]);
				// --------------------------------------------------------------------
				// CLEAN-UP - Free the allocated memory.  We don't need it any longer
				// --------------------------------------------------------------------
				free(pHead);
				free(pM3);

			}
			else {
				printf("CHILD PID# %d  I am worker # %d and row %d does not EXIST so I DO NOTHING!!!\n", getpid(), i, i);								
			}
		}
		else {
			printf("CHILD PID# %d :  I am worker %d and there are only %d rows so I do NOTHING!\n", getpid(), i, n1);
		}
		break;
		// exit(0);
	  }
	  /**
	    * -----------------  PARENT PROCESSING -----------------
		*/
	else{
 	    fprintf(stderr, "*** PARENT PID# %d - fork(): Returned PID %d  (myParentPID # %d) \n", getpid(), (int) pid, getppid());
		*pChildPidTemp++ = pid;
		if( n1 < matMul->nWorkers){
			printf("PARENT PID# %d  FEWER ROWS THAN WORKERS!  Number of M1 rows = %d   Number of workers = %d \n", getpid(), n2, matMul->nWorkers);

			if (i < n1 ){
				printf("PARENT PID# %d  I am worker # %d and this is row %d so I MUST WORK!\n", getpid(), i, i);

				close(pInPipeFDArray[i][READ]);
				close(pOutPipeFDArray[i][WRITE]);	  
				
				for ( j = 0; j < n2; j++ ){
					printf("PARENT PID# %d WRITING:  a[%d][%d] = %d\n",getpid(), i, j, a[i][j]);
				}

				// http://stackoverflow.com/questions/5237041/how-to-send-integer-with-pipe-between-two-processes
				// Write the designated M1 row to the pipe
				for ( j = 0; j < n2; j++ ){
					write( pInPipeFDArray[i][WRITE], &(a[i][j]), sizeof( a[i][j]) );
				}
				for ( j = 0; j < n2; j++ ){
					for( k = 0; k < n3; k++ ) {
						write( pInPipeFDArray[i][WRITE], &(b[j][k]), sizeof( b[j][k]) );
						printf("PARENT PID# %d WRITING:  b[%d][%d] = %d\n",getpid(), j, k, b[j][k]);
					}
				}
				// Write the entire M2 to the pipe
				close(pInPipeFDArray[i][WRITE]);
				
				waitpid(pid, &status,0 );
				fprintf(stderr, "**A EXIT STATUS** CHILD PID# %d :  WIFEXITED Status = %d\n",   (int)pid, WIFEXITED(&status));
				fprintf(stderr, "**A EXIT STATUS** CHILD PID# %d :  WEXITSTATUS Status = %d\n", (int)pid, WEXITSTATUS(&status));
				fprintf(stderr, "**A EXIT STATUS** CHILD PID# %d :  WIFSIGNALED Status = %d\n", (int)pid, WIFSIGNALED(&status));
				fprintf(stderr, "**A EXIT STATUS** CHILD PID# %d :  WTERMSIG Status = %d\n",    (int)pid, WTERMSIG(&status));
//				fprintf(stderr, "**A EXIT STATUS** CHILD PID# %d :  WIFSTIPPED Status = %d\n",  (int)pid, WIFSTIPPED(&status));
				fprintf(stderr, "**A EXIT STATUS** CHILD PID# %d :  WSTOPSIG Status = %d\n",    (int)pid, WSTOPSIG(&status));
				
				/* Read each contents of each outpipe into the output matrix  */
				MatrixBaseType *pElement = &(c[i][0]);
				for( j=0; j<n3; j++){
					n = read( pOutPipeFDArray[i][READ], pElement, sizeof(int) );
					printf("EPILOG PID# %d :  Read %d from Outpipipe[%d][%d] \n", getpid(), *pElement, i,j);
					pElement++;
				}	
		    }
			else {
				printf("PARENT PID# %d  I am worker # %d and row %d does not EXIST so I DO NOTHING!!!\n", getpid(), i, i);				
				waitpid(pid, &status, 0);
				fprintf(stderr, "**B EXIT STATUS** CHILD PID# %d :  WIFEXITED Status = %d\n",   (int)pid, WIFEXITED(&status));
				fprintf(stderr, "**B EXIT STATUS** CHILD PID# %d :  WEXITSTATUS Status = %d\n", (int)pid, WEXITSTATUS(&status));
				fprintf(stderr, "**B EXIT STATUS** CHILD PID# %d :  WIFSIGNALED Status = %d\n", (int)pid, WIFSIGNALED(&status));
				fprintf(stderr, "**B EXIT STATUS** CHILD PID# %d :  WTERMSIG Status = %d\n",    (int)pid, WTERMSIG(&status));
//				fprintf(stderr, "**B EXIT STATUS** CHILD PID# %d :  WIFSTIPPED Status = %d\n",  (int)pid, WIFSTIPPED(&status));
				fprintf(stderr, "**B EXIT STATUS** CHILD PID# %d :  WSTOPSIG Status = %d\n",    (int)pid, WSTOPSIG(&status));
			}
		}
		else {
			printf("PARENT PID# %d :  I am worker %d and there are only %d rows so I do NOTHING!\n", getpid(), i, n1);
			waitpid(pid, &status, 0);
			fprintf(stderr, "**C EXIT STATUS** CHILD PID# %d :  WIFEXITED Status = %d\n",   (int)pid, WIFEXITED(&status));
			fprintf(stderr, "**C EXIT STATUS** CHILD PID# %d :  WEXITSTATUS Status = %d\n", (int)pid, WEXITSTATUS(&status));
			fprintf(stderr, "**C EXIT STATUS** CHILD PID# %d :  WIFSIGNALED Status = %d\n", (int)pid, WIFSIGNALED(&status));
			fprintf(stderr, "**C EXIT STATUS** CHILD PID# %d :  WTERMSIG Status = %d\n",    (int)pid, WTERMSIG(&status));
//			fprintf(stderr, "**C EXIT STATUS** CHILD PID# %d :  WIFSTIPPED Status = %d\n",  (int)pid, WIFSTIPPED(&status));
			fprintf(stderr, "**C EXIT STATUS** CHILD PID# %d :  WSTOPSIG Status = %d\n",    (int)pid, WSTOPSIG(&status));	
		}
		free( pInPipeFDArray );
		free( pOutPipeFDArray);
		
		// FOR DEBUG -- this is what matrix c[][] looks like at end.
		for(i=0; i<n1; i++){
				printf("\n");
				for(j=0; j<n3; j++){
					printf("%d ",c[i][j]);
			    }
		    printf("\n");
		}   /* if( n1 < matMul->nWorkers) */
    }  /* If Parent/Child */
  }  /* for( i = 0; i < matMul->nWorkers; i++ ) */
  
}
