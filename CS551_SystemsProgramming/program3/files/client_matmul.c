#include "common.h"
#include "matmul.h"

#include "errors.h"
#include "trace.h"
#include "errors.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

//#define DO_TRACE 1
#include "trace.h"

// define the opaque MatrixMul type
struct MatrixMul{
	pid_t		pid;
	FILE 		*trace;
	char 		*pWkServerFifo;		// well-known server fifo and path
	int 		 wkServerFd;		// well-known server FIFO fd
	char 		*pServerFifo;		// private named server fifo - name strlen
	int 		 serverFd;			// private named server fifo fd
	char 		*pClientFifo;		// private named client fifo - name string
	int 		 clientFd;			// private named client fifo fd
};


// -------------------------------------------------------------------------------------
// handleServerError
// -------------------------------------------------------------------------------------
// Attempts to read and display server error message from Fifo.
// -------------------------------------------------------------------------------------
int handleServerError( const MatrixMul *pMM, MsgHeader_T *pServerResponseMsg){
	
	pid_t 	pid = getpid();
	char 	serverErrMsg[MSG_STR_MAX] = {0};
	int 	status = SUCCESS;
	
	if( pServerResponseMsg->code == SERVER_ERROR){
		fprintf(stderr,  "Client PID # %d received Server Code [0x%08X] Error: 0x%08X", pid, pServerResponseMsg->code, pServerResponseMsg->errCode);
		// Now try to read the server's error message
		if( read(pMM->clientFd, serverErrMsg, pServerResponseMsg->len) != pServerResponseMsg->len ){
			fprintf(stderr,  "CLIENT PID # %d handleServerError : reading from %s error ", pid, pMM->pClientFifo);
			status = EPIPE;
		}
		fprintf(stderr,  "%s\n", serverErrMsg);
	}
	return status;
}

/** Return an interface to the client end of a client-server matrix
 *  multiplier set up to multiply using multiplication module
 *  specified by modulePath with server daemon running in directory
 *  serverDir.
 *
 *  If modulePath is relative, then it must be found on the server's
 *  LD_LIBRARY_PATH interpreted relative to serverDir.  The name of
 *  the multiplication function in the loaded module is the last
 *  component of the modulePath with the extension (if any) removed.
 *
 *  If trace is non-NULL, then turn on tracing for all subsequent
 *  calls to mulMatrixMul() which use the returned MatrixMul.
 *  Specifically, after completing each matrix multiplication, the
 *  client should log a single line on trace in the format:
 *
 *  utime: UTIME, stime: STIME, wall: WALL
 *
 *  where UTIME, STIME and WALL gives the amount of user time, system
 *  time and wall time in times() clock ticks needed within the server
 *  to perform only the multiplication function provided by the
 *  module. The spacing must be exactly as shown above and all the
 *  clock tick values must be output in decimal with no leading zeros
 *  or redundant + signs.
 *
 *  Set *err to an appropriate error number (documented in errno(3))
 *  on error.
 *
 *  This call should result in the creation of a new worker process on
 *  the server, spawned using the double-fork technique.  The worker
 *  process must load and link the specified module.  All future
 *  multiplication requests on the returned MatrixMul must be
 *  performed using the specified module within this worker process.
 *  All IPC between the client and server processes must be performed
 *  using only named pipes (FIFO's).
 */
MatrixMul *
newMatrixMul(const char *serverDir, const char *modulePath,
             FILE *trace, int *err)
{
	pid_t 				pid  = getpid();
	MatrixMul 		   *pMM  = NULL; 
	char 				tempFifoName[PRIVATE_FIFO_NAME_LEN] = {0};
	MsgHeader_T			newClientMsg = {0}, serverResponseMsg = {0};
	
	pMM = calloc( 1, sizeof(MatrixMul));
	if( test_malloc_ptr(pMM, err) != SUCCESS ) {
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: pMM calloc error ", pid);
		goto NEW_MATRIX_MUL_LABEL_00;
	}  	
	
	// initialize trace
	if( trace != NULL ){ pMM->trace = trace; }
		
	// Set-up the MatrixMul server path (alloc memory, copy the path to it, add well-known fifo name )
	pMM->pWkServerFifo = calloc(1, strlen(serverDir) + strlen(SERVER_FIFO) + 2);		// one for Null and one for possible final / on path
	if( test_malloc_ptr(pMM->pWkServerFifo, err) != SUCCESS ) {
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: pMM->pWkServerFifo calloc error ", pid);
		goto NEW_MATRIX_MUL_LABEL_10;
	}  
    strcpy( pMM->pWkServerFifo, serverDir);
	checkFilePath(pMM->pWkServerFifo);	
    strcat( pMM->pWkServerFifo, SERVER_FIFO );
    TRACE("\npMM->pWkServerFifo and Server = %s\n", pMM->pWkServerFifo);
    
	
	// ------------------------------------------------------------------------------------------------------------------------
    // Create and open our FIFOs before sending out the request to the server to avoid a race condition    
	// SERVER's private named FIFO
	// allocate memory and copy the path and name to the new memory
	pMM->pServerFifo = calloc(1, strlen(serverDir) + PRIVATE_FIFO_NAME_LEN + 2);		// one for Null and one for possible final / on path
	if( test_malloc_ptr(pMM->pServerFifo, err) != SUCCESS ) {
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: pMM->pServerFifo calloc error \n", pid);
		goto NEW_MATRIX_MUL_LABEL_20;
	}  
    strcpy( pMM->pServerFifo, serverDir);
	checkFilePath(pMM->pServerFifo);	
	get_private_fifo_name( SERVER, pid, tempFifoName );
	strcat( pMM->pServerFifo, tempFifoName );
	TRACE("NEW MATRIX MUL PID # %d:  pMM->pServerFifo = %s", pid, pMM->pServerFifo);
    if( (mkfifo(pMM->pServerFifo, S_IRUSR | S_IWUSR | S_IWGRP) == ERROR ) && errno != EEXIST ){
		fprintf(stderr,  "PID # %d newMatrixMull: make FIFO %s error \n", pid, pMM->pServerFifo );
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_30;
    }

	// CLIENT's private named FIFO
	// allocate memory and copy the path and name to the new memory
	pMM->pClientFifo = calloc(1, strlen(serverDir) + PRIVATE_FIFO_NAME_LEN + 2);		// one for Null and one for possible final / on path
	if( test_malloc_ptr(pMM->pClientFifo, err) != SUCCESS ) {
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: pMM->pClientFifo calloc error \n", pid);
		goto NEW_MATRIX_MUL_LABEL_30;
	}  
    strcpy( pMM->pClientFifo, serverDir);
	checkFilePath(pMM->pClientFifo);	
	get_private_fifo_name( CLIENT, pid, tempFifoName );
	strcat( pMM->pClientFifo, tempFifoName );
	TRACE("NEW MATRIX MUL PID # %d:  pMM->pClientFifo = %s", pid, pMM->pClientFifo);
    if( (mkfifo(pMM->pClientFifo, S_IRUSR | S_IWUSR | S_IWGRP) == ERROR ) && errno != EEXIST ){
		fprintf(stderr, "PID # %d newMatrixMull: make FIFO %s error \n", (int) pid, pMM->pClientFifo );
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  
	}
		
	// Don't open the private client fifo until we are ready to block until data is available
	// ------------------------------------------------------------------------------------------------------------------------
    // open well-known server FIFO
    pMM->wkServerFd = open(pMM->pWkServerFifo, O_WRONLY);
    if( pMM->wkServerFd == ERROR ){ 
		fprintf( stderr, "CLIENT PID # %d newMatrixMull: opening %s error \n", pid, pMM->pWkServerFifo);
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  
    }
    
    // write our pid to the server FIFO to let it know that we exist 
    if( write(pMM->wkServerFd, &pid, sizeof(pid_t)) != sizeof(pid_t) ){
		fprintf( stderr, "CLIENT PID # %d newMatrixMull: error writing pid to %s ", pid, pMM->pWkServerFifo);
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  
    }

	// Now, open private fifos for reading and writing
  	pMM->serverFd = open( pMM->pServerFifo, O_WRONLY );
	if( pMM->serverFd <= ERROR ){
		fprintf( stderr, "CLIENT PID # %d newMatrixMull: opening private fifo:  %s ", pid, pMM->pServerFifo);
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  
	}
	
	// package the module and its path if present and send it to the server's worker thread
	newClientMsg.code = NEW_CLIENT;
	newClientMsg.pid = pid;
	newClientMsg.len = strlen(modulePath) + 1;
	
	// First write the message header 
    if( write(pMM->serverFd, &newClientMsg, MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: writing ModulePath_Packet to %s error ", pid, pMM->pServerFifo);
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  		
    }
	// Next write the module path
    if( write(pMM->serverFd, modulePath, newClientMsg.len) != newClientMsg.len ){
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: writing ModulePath_Packet to %s error ", pid, pMM->pServerFifo);
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  		
    }
	
	// OK, now we are ready to open the client Fifo and wait for the server's response
	pMM->clientFd = open( pMM->pClientFifo, O_RDONLY );
	if( pMM->clientFd <= ERROR){
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: error opening %s for READING ", pid, pMM->pClientFifo );
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;  				
	}
	
	// read the response back from the client FIFO
    if( read(pMM->clientFd, &serverResponseMsg, MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
		fprintf(stderr,  "CLIENT PID # %d newMatrixMull: writing ModulePath_Packet to %s error ", pid, pMM->pServerFifo);
		*err = EPIPE;
		goto NEW_MATRIX_MUL_LABEL_40;
    }
	
	if( serverResponseMsg.code == SERVICE_READY) {
		TRACE("Client PID # %d - newMatrixMul: (SERVICE_READY)", pid);
		return pMM;
	}

// =====================  Error Handling  =====================

	handleServerError( pMM, &serverResponseMsg);
	*err = serverResponseMsg.errCode;

NEW_MATRIX_MUL_LABEL_40:
	if( pMM->pClientFifo != NULL ){ free (pMM->pClientFifo); }

NEW_MATRIX_MUL_LABEL_30:  
	if( pMM->pServerFifo != NULL){ free (pMM->pServerFifo);}
	
NEW_MATRIX_MUL_LABEL_20:  
	if( pMM->pWkServerFifo != NULL) { free( pMM->pWkServerFifo); }
	
NEW_MATRIX_MUL_LABEL_10:  
	if( pMM != NULL ){ free(pMM); }
	
NEW_MATRIX_MUL_LABEL_00:
  return NULL;
}

/** Free all resources used by matMul.  Specifically, free all memory
 *  used by matMul, remove all FIFO's created specifically for matMul
 *  and set up the worker process on the server to terminate.  Set
 *  *err appropriately (as documented in errno(3)) on error.
 */
void freeMatrixMul(MatrixMul *matMul, int *err)
{
	
	int 	status = SUCCESS, n = 0;
	pid_t 	pid = getpid();
	char 	dummy_byte;
	
	// Close the pipes we opened
	TRACE("freeMatrixMul PID # %d:  Closing %s, %s, and %s",  pid, matMul->pServerFifo, matMul->pClientFifo, matMul->pWkServerFifo );
	
	// Close the server Fifo  which we opened WR-ONLY
	status = close( matMul->serverFd ); 
	if(status < SUCCESS){
		*err = EPIPE;
		fprintf(stderr, "freeMatrixMul PID # %d:  error closing %s fd (%d)", pid, matMul->pServerFifo, matMul->serverFd);
	}

	// Close our connection to the well-known Fifo (also opened write-only)
	status = close( matMul->wkServerFd ); 
	if(status < SUCCESS){
		*err = EPIPE;
		fprintf(stderr, "freeMatrixMul PID # %d:  error closing %s fd (%d)", pid, matMul->pWkServerFifo, matMul->wkServerFd);
	}
	
	// Read from the client fifo until it returns EOF (this signals that the server closed its WR-ONLY end of this Fifo)
	while( (n = read(matMul->clientFd, &dummy_byte, 1)) != PIPE_EOF );
		
	// Close the client Fifo
	status = close( matMul->clientFd ); 
	if(status < SUCCESS){
		*err = EPIPE;
		fprintf(stderr, "freeMatrixMul PID # %d:  error closing %s fd (%d)", pid, matMul->pClientFifo, matMul->clientFd);
	}
	
	// Remove the named pipes.  
	*err = remove( matMul->pServerFifo);
	*err = remove( matMul->pClientFifo);
	
	// Free the memory holding the pipe names
	TRACE("freeMatrixMul PID # %d:  freeing memory for  %s, %s, and %s",pid, matMul->pServerFifo, matMul->pClientFifo,matMul->pWkServerFifo );
	if(matMul->pServerFifo != NULL ){ free(matMul->pServerFifo); }
	if(matMul->pClientFifo != NULL ){ free(matMul->pClientFifo); }
	if(matMul->pWkServerFifo != NULL ){ free(matMul->pWkServerFifo); }
	
	TRACE("freeMatrixMul PID # %d:  freeing matMul", pid );
	if(matMul != NULL){ free(matMul); }

}

/** Set matrix c[n1][n3] to a[n1][n2] * b[n2][n3].  It is assumed that
 *  the caller has allocated c[][] appropriately.  Set *err to an
 *  appropriate error number (documented in errno(3)) on error.  If
 *  *err is returned as non-zero, then the matMul object may no longer
 *  be valid and future calls to mulMatrixMul() may have unpredictable
 *  behavior.  It is the responsibility of the caller to call
 *  freeMatrixMul() after an error.
 *
 *  The multiplication must be entirely on the server using the
 *  specified module by the worker process which was spawned when
 *  matMul was created.  Note that a single matMul instance may be
 *  used for performing multiple multiplications.  All IPC must be
 *  handled using FIFOs.
 */
void
mulMatrixMul(const MatrixMul *matMul, int n1, int n2, int n3,
             CONST MatrixBaseType a[n1][n2],
             CONST MatrixBaseType b[n2][n3],
             MatrixBaseType c[n1][n3], int *err)
{
	
	pid_t 		      pid = getpid();
	int 			  n = 0;
	MsgHeader_T		  newMulRequestMsg = {0};
	MsgHeader_T		  MatrixAMsg = {0};
	MsgHeader_T		  MatrixBMsg = {0};
	char 			  timeString[MSG_STR_MAX] = {0};
	
	// Set up the new multiplication request message
	newMulRequestMsg.code = NEW_PROBLEM;
	newMulRequestMsg.pid  = pid;
	newMulRequestMsg.n1   = n1;
	newMulRequestMsg.n2   = n2;
	newMulRequestMsg.n3   = n3;

	if( write(matMul->serverFd, &newMulRequestMsg,MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
		*err = EPIPE;
		fprintf(stderr, "Client PID # %d: Failed writing newMulRequestMsg.\n", pid);
		return;
	}
	
	// Set up the matrix A message
	MatrixAMsg.code = A_MATRIX;
	MatrixAMsg.pid  = pid;
	MatrixAMsg.len  = n1 * n2 * SIZEOF_MBT;
	MatrixAMsg.n1   = n1;
	MatrixAMsg.n2   = n2;
	MatrixAMsg.n3   = n3;

	if( write(matMul->serverFd, &MatrixAMsg, MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
		*err = EPIPE;
		fprintf(stderr, "Client PID # %d: Failed writing MatrixAMsg.\n", pid);
		return;
	}	
	
	// Write Matrix A 
	if( write(matMul->serverFd, a, MatrixAMsg.len) != MatrixAMsg.len ){
		*err = EPIPE;
		fprintf(stderr, "Client PID # %d: Failed writing MatrixA data.\n", pid);
		return;
	}
	
	// Set up the matrix B message
	MatrixBMsg.code = B_MATRIX;
	MatrixBMsg.pid  = pid;
	MatrixBMsg.len  = n2 * n3 * SIZEOF_MBT;
	MatrixBMsg.n1   = n1;
	MatrixBMsg.n2   = n2;
	MatrixBMsg.n3   = n3;

	if( write(matMul->serverFd, &MatrixBMsg, MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
		*err = EPIPE;
		fprintf(stderr, "Client PID # %d: Failed writing MatrixAMsg.\n", pid);
		return;
	}	
	
	// Write Matrix B
	if( write(matMul->serverFd, b, MatrixBMsg.len) != MatrixBMsg.len ){
		*err = EPIPE;
		fprintf(stderr, "Client PID # %d: Failed writing MatrixB data.\n", pid);
		return;
	}
	
	// OK, now we wait for an answer from the server	
	int sizeM3 = n1 * n3 * SIZEOF_MBT;
	if( (n = read(matMul->clientFd, c, sizeM3 )) !=  sizeM3 ){
		*err = EPIPE;
		fprintf(stderr, "Client PID # %d: Failed reading c (Got %d bytes.  Expected %d bytes.)\n", pid, n, sizeM3);
	}
	else { 
		myOutMatrix( stderr, n1, n3, c, "Client returning this result:");
		if( (n = read(matMul->clientFd, timeString, MSG_STR_MAX )) !=  MSG_STR_MAX ){
			*err = EPIPE;
			fprintf(stderr, "Client PID # %d: Failed reading c (Got %d bytes.  Expected %d bytes.)\n", pid, n, sizeM3);
		}
		if( matMul->trace != NULL ){
			fprintf(matMul->trace, "%s",timeString);
		}
		
	}
	return;  	
}
