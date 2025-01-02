#include "common.h"
#include "mat_base.h"

//#define DO_TRACE 1
#include "trace.h"
#include "errors.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h>

// ======================== SERVER TYPES ==============================
typedef struct WorkerInfo_TYPE{
	char *				pModuleName;	// May also include path name (and ends in '.mod)
	char *  			pModuleSymbol;  // Module name as found in the symbol table
	void *				pModHandle;		// handle to module returned by dlopen()
	int 				serverFd;		// this is the pipe the server READS from
	int					clientFd;		// this is the pipe the client READS from
	int 				dummyFd;		// see Kerrisk p.912 sample program
} WorkerInfo_T;


// ======================== SERVER FUNCTIONS ==============================

// -------------------------------------------------------------------------------------
// reportErrorToClient
// -------------------------------------------------------------------------------------
// Reports server side errors to client.  
// Abort (kill this worker thread but daemon persists) if write fails.  It makes no 
// sense to continue if client/server communication is down.
// -------------------------------------------------------------------------------------
static void reportErrorToClient( WorkerInfo_T *pWorkerInfo, int *pErrCode, char *errStr ){
	MsgHeader_T 	errMsg = {0};
	pid_t 			pid = getpid();
	int				len = strlen(errStr) + 1;
	
	errMsg.code 	= SERVER_ERROR;
	errMsg.pid  	= pid;
	errMsg.errCode	= *pErrCode;
	
	// Write the message header to client fifo
	if( write(pWorkerInfo->clientFd, &errMsg, MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
		fatal("reportErrorToClient PID # %d:  error writing message:  %s  [code 0x%08X] ", pid, errStr, *pErrCode );
	}
	// Write the error message string to client fifo
	if( write(pWorkerInfo->clientFd, errStr, len) != len ){
		fatal("reportErrorToClient PID # %d:  error writing message:  %s  [code 0x%08X] ", pid, errStr, *pErrCode );
	}
}

// -------------------------------------------------------------------------------------
// getElapsedTime
// -------------------------------------------------------------------------------------
// Returns a formatted string with the user, system, and wall time. (String needs to
// be freed by caller.)
// -------------------------------------------------------------------------------------
static char *getElapsedTime( WorkerInfo_T *pWorkerInfo, clock_t elapsed, const struct tms *ptBegin, const struct tms *ptEnd){
	
	char	*pTimeString;
	char 	errStr[MSG_STR_MAX];
	int		status;
	pid_t 	pid = getpid();
	
	int tics_per_second = sysconf(_SC_CLK_TCK);
	
	pTimeString = calloc( 1, MSG_STR_MAX);
	if( test_malloc_ptr( pTimeString, &status )!= SUCCESS ){
		snprintf( errStr, MSG_STR_MAX, "getElapsedTime PID # %d - pTimeString calloc failed. ", pid );
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}
	
	long 	userTime = (ptEnd->tms_utime - ptBegin->tms_utime)/tics_per_second;
	long 	sysTime = (ptEnd->tms_stime - ptBegin->tms_stime)/tics_per_second;	
	snprintf(pTimeString, MSG_STR_MAX,"utime: %ld, stime: %ld, wall: %ld\n", userTime, sysTime, elapsed/tics_per_second  );
	TRACE( "%s", pTimeString );
	return pTimeString;
}

// -------------------------------------------------------------------------------------
// setupNewClient
// -------------------------------------------------------------------------------------
// Initialize the information for this worker thread (extract and load the requested
// matrix library function)
// -------------------------------------------------------------------------------------
static int setupNewClient( WorkerInfo_T *pWorkerInfo, char *pData ){

	char 		errStr[MSG_STR_MAX] = {0};
	char 		msgStr[MSG_STR_MAX] = {0};
	MsgHeader_T	serverMsg			= {0};				/* struct written to client fifo */
	int 		len 				= strlen(pData) + 1;
	pid_t		pid 				= getpid();
	int 		status 				= SUCCESS;
	
	// Store name of request module (include suffix - ".mod")
	pWorkerInfo->pModuleName = calloc(1,  len );
	if( test_malloc_ptr( pWorkerInfo->pModuleName, &status )!= SUCCESS ){
		snprintf( errStr, MSG_STR_MAX, "setupNewClient PID # %d - pModuleName calloc failed. ", pid);
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}
	strncpy( pWorkerInfo->pModuleName, pData, len );

	// Extract the symbol name from the module
	pWorkerInfo->pModuleSymbol = calloc(1, len );
	if( test_malloc_ptr( pWorkerInfo->pModuleSymbol, &status )!= SUCCESS ){
		snprintf( errStr, MSG_STR_MAX, "setupNewClient PID # %d - pModuleSymbol calloc failed. ", pid);
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}
	strcpy( pWorkerInfo->pModuleSymbol, pWorkerInfo->pModuleName);
	char *pChar = strchr(pWorkerInfo->pModuleSymbol, '.');
	if(pChar != NULL) { *pChar = '\0'; 	} 
	
	// Use dlopen() to open the requested matrix library  function
	TRACE("setupNewClient:  module symbol =  %s",pWorkerInfo->pModuleSymbol);				
	TRACE("setupNewClient:  opening shared module %s",pWorkerInfo->pModuleName);
	errno = 0;
	dlerror();		// clear dlerror just in case

	if( (pWorkerInfo->pModHandle = dlopen( pWorkerInfo->pModuleName, RTLD_NOW | RTLD_GLOBAL )) == NULL ){					
		snprintf(errStr, MSG_STR_MAX, "setupNewClient PID %d: Bad dlopen() call:  %s ", getpid(), dlerror() );
		status = ELIBACC;
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}
	else {
		snprintf(msgStr, MSG_STR_MAX, "Server Worker PID # %d READY:  %s library function loaded. ", pid, pWorkerInfo->pModuleSymbol);
		serverMsg.code = SERVICE_READY;
		serverMsg.pid  = pid;
		serverMsg.len  = strlen(msgStr) + 1;
		// First write the message to the client's pipe
		if( write(pWorkerInfo->clientFd, &serverMsg, MSG_HEADER_SIZE) != MSG_HEADER_SIZE ){
			snprintf(errStr, MSG_STR_MAX, "setupNewClient PID # %d:  error writing message:  [code 0x%08X] ", pid, serverMsg.errCode );
			status = EPIPE;
			reportErrorToClient( pWorkerInfo, &status, errStr);
		}
	}
	return status;
}

// -------------------------------------------------------------------------------------
// setupNewProblem
// -------------------------------------------------------------------------------------
// Initialize the matrix dimensions and allocate memory to hold the matrix data.  
// Report errors (if any) to client.
// -------------------------------------------------------------------------------------
static int  setupNewProblem(MsgHeader_T  *pNewClientMsg, WorkerInfo_T *pWorkerInfo, MulProblem_T *pMulProblem ){
	
	int   status 			  = SUCCESS;
	char  errStr[MSG_STR_MAX] = {0};
	pid_t pid 				  = getpid();
	
	// Setup matrix dimensions
	pMulProblem->n1 = pNewClientMsg->n1;
	pMulProblem->n2 = pNewClientMsg->n2;
	pMulProblem->n3 = pNewClientMsg->n3;

	// Allocate memory to hold the matrix data
	pMulProblem->sizeM1 = pMulProblem->n1 * pMulProblem->n2 * SIZEOF_MBT;
	pMulProblem->pM1 = calloc( 1, pMulProblem->sizeM1);
	if( test_malloc_ptr( pMulProblem->pM1, &status )!= SUCCESS ){
		snprintf( errStr, MSG_STR_MAX, "setupNewProblem PID # %d - pM1 calloc failed. ", pid);
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}

	pMulProblem->sizeM2 = pMulProblem->n2 * pMulProblem->n3 * SIZEOF_MBT;
	pMulProblem->pM2 = calloc( 1, pMulProblem->sizeM2 );
	if( test_malloc_ptr( pMulProblem->pM2, &status )!= SUCCESS ){
		snprintf( errStr, MSG_STR_MAX, "setupNewProblem PID # %d - pM2 calloc failed. ", pid);
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}

	pMulProblem->sizeM3 = pMulProblem->n1 * pMulProblem->n3 * SIZEOF_MBT;
	pMulProblem->pM3 = calloc( 1, pMulProblem->sizeM3 );
	if( test_malloc_ptr( pMulProblem->pM3, &status )!= SUCCESS ){
		snprintf( errStr, MSG_STR_MAX, "setupNewProblem PID # %d - pM3 calloc failed. ", pid);
		reportErrorToClient( pWorkerInfo, &status, errStr );
	}
	return status;
}

// -------------------------------------------------------------------------------------
// executeMultiply
// -------------------------------------------------------------------------------------
// Pass the matrix multiplication data to the requested shared library and return the
// result (or error) to the client.
// -------------------------------------------------------------------------------------
static void executeMultiply( WorkerInfo_T *pWorkerInfo, MulProblem_T *pMulProblem){	
	const char       *dlerror_str = NULL;
	char 			 *pTimeString = NULL;
	struct tms		 tmsBegin = {0}, tmsEnd = {0};
	clock_t 		 tBegin, tEnd;	
	char  			 errStr[MSG_STR_MAX] = {0};
	pid_t 			 pid = getpid();
	int 			 status = SUCCESS;
	int 			 n1 = pMulProblem->n1;
	int 			 n2 = pMulProblem->n2;
	int 			 n3 = pMulProblem->n3;
	void 			 (*funcp)(int, int, int, CONST MatrixBaseType[n1][n2], CONST MatrixBaseType[n2][n3], CONST MatrixBaseType[n1][n3], int*);

	// Get start timing data
	if( (tBegin = times(&tmsBegin)) < 0 ){
		snprintf(errStr, MSG_STR_MAX, "executeMultiply PID %d: Error reading starting time (times()) ", getpid() );
		status = EDOM;
		reportErrorToClient( pWorkerInfo, &status, errStr );				
	}
	
	(void) dlerror(); 		/* clear dlerror() */
	*(void **) (&funcp) = dlsym( pWorkerInfo->pModHandle, pWorkerInfo->pModuleSymbol);
	dlerror_str = dlerror();
	if( dlerror_str != NULL){
		snprintf(errStr, MSG_STR_MAX, "executeMultiply PID %d: Bad dlsym() call:  %s (module symbol = %s) ", getpid(), dlerror(), pWorkerInfo->pModuleSymbol);
		status = ELIBACC;
		reportErrorToClient( pWorkerInfo, &status, errStr );		
	}
	status = 0;
	(*funcp)(n1, n2, n3, ( MatrixBaseType (*)[n2] ) pMulProblem->pM1,
						 ( MatrixBaseType (*)[n3] ) pMulProblem->pM2,
						 ( MatrixBaseType (*)[n3] ) pMulProblem->pM3, &status);
	
	// Get end timing data
	if( (tEnd = times(&tmsEnd)) < 0 ){
		snprintf(errStr, MSG_STR_MAX, "executeMultiply PID %d: Error reading end time (times()) ", getpid() );
		status = EDOM;
		reportErrorToClient( pWorkerInfo, &status, errStr );				
	}

	if( status == SUCCESS ){
		if( write(pWorkerInfo->clientFd, pMulProblem->pM3, pMulProblem->sizeM3) != pMulProblem->sizeM3 ){
			snprintf(errStr, MSG_STR_MAX, "executeMultiply PID # %d:  error writing M3 to client pipe.", pid );
			status = EPIPE;
			reportErrorToClient( pWorkerInfo, &status, errStr);
		}
		pTimeString = getElapsedTime( pWorkerInfo, (tEnd-tBegin), &tmsBegin, &tmsEnd);
		if( write(pWorkerInfo->clientFd, pTimeString, MSG_STR_MAX) != MSG_STR_MAX ){
			snprintf(errStr, MSG_STR_MAX, "executeMultiply PID # %d:  error writing pTimeString to client pipe.", pid );
			status = EPIPE;
			reportErrorToClient( pWorkerInfo, &status, errStr);
		}
		free(pTimeString);
	}
	else {
			snprintf(errStr, MSG_STR_MAX, "executeMultiply PID # %d:  error in multiplication function.", pid );
			reportErrorToClient( pWorkerInfo, &status, errStr);		
	}
}

// ---------------------------------------------------------------------------------------------------------
// cleanUpProblem
// ---------------------------------------------------------------------------------------------------------
// The matrix multiplication for this problem is done so free memory used for the problem and 
// reset the matrix multiplication data structure
// ---------------------------------------------------------------------------------------------------------
static int cleanUpProblem( WorkerInfo_T *pWorkerInfo, MulProblem_T *pMulProblem ){
	
	if( pMulProblem->pM1 != NULL ){ free( pMulProblem->pM1); }
	if( pMulProblem->pM2 != NULL ){ free( pMulProblem->pM2); }
	if( pMulProblem->pM3 != NULL ){ free( pMulProblem->pM3); }
	
	pMulProblem->n1 = 0;
	pMulProblem->n2 = 0;
	pMulProblem->n3 = 0;

	pMulProblem->sizeM1 = 0;
	pMulProblem->sizeM2 = 0;
	pMulProblem->sizeM3 = 0;
	
	return SUCCESS;
	
}


// ---------------------------------------------------------------------------------------------------------
// performWorkerCleanup
// ---------------------------------------------------------------------------------------------------------
// Call this function when a read from the private server Fifo returns 0 which signals that the client
// has called freeMatrixMul and no longer wishes to do matrix multiplication.  This function closes
// pipes and frees memory.
// ---------------------------------------------------------------------------------------------------------
static void performWorkerCleanup( WorkerInfo_T *pWorkerInfo ){
	
	int 	status = SUCCESS;
	pid_t 	pid = getpid();

	TRACE("performWorkerCleanup PID # %d:  Closing pWorkerInfo->serverFd.",pid);
	status = close( pWorkerInfo->serverFd );
	if( status != SUCCESS ){
		fatal("PID # %d doWorkerService: Error closing serverFd.", pid);
	}

	TRACE("performWorkerCleanup PID # %d:  Closing pWorkerInfo->clientFd.",pid);
	status = close( pWorkerInfo->clientFd );
	if( status != SUCCESS ){
		fatal("PID # %d doWorkerService: Error closing clientFd.", pid);
	}
	
	// Free Message and WorkerInfo memory
	if( pWorkerInfo->pModuleName != NULL ){ free(pWorkerInfo->pModuleName); }
	if( pWorkerInfo->pModuleSymbol != NULL ){ free(pWorkerInfo->pModuleSymbol); }
}

// ---------------------------------------------------------------------------------------------------------
// doWorkerService
// ---------------------------------------------------------------------------------------------------------
// This routine is run for each server worker process.
// It is responsible for opening fds for the private named pipes used to communicate between the server
// and client.
// ---------------------------------------------------------------------------------------------------------
static void doWorkerService( pid_t clientPid, const char *serverDir ){
	
	TRACE("doing WorkerService....");
	
	// -------- NEW Interface ---------------
	MsgHeader_T			*pNewClientMsg;
	WorkerInfo_T		workerInfo = {0};
	int 				n = 0,  status = SUCCESS ;
	char  				sFifoName[PRIVATE_FIFO_NAME_LEN], cFifoName[PRIVATE_FIFO_NAME_LEN]; // Names of private server and client fifos
	MulProblem_T 		mulProblem = {0};
	char  				errStr[MSG_STR_MAX] = {0};
	pid_t 				pid = getpid();
	char 				*pData = NULL;

	chdir( serverDir );
	
	// --------------------------------------------------------------------------------------------------
	// Assume that private fifos are in the same directory as the daemon and
	// that this daemon directory is the cwd.  Errors opening the pipes between the 
	// client and server are fatal because there is no way to report them back 
	// without operational pipes.
	// --------------------------------------------------------------------------------------------------
	get_private_fifo_name( SERVER, clientPid, sFifoName );
	workerInfo.serverFd = open(sFifoName, O_RDONLY );
	if( workerInfo.serverFd == ERROR ){
		fatal("PID # %d doWorkerService: Error opening private named FIFO: %s", getpid(), sFifoName);
	}
	get_private_fifo_name( CLIENT, clientPid, cFifoName );
	workerInfo.clientFd = open(cFifoName, O_WRONLY );
	if( workerInfo.clientFd == ERROR ){
		fatal("PID # %d doWorkerService: Error opening private named FIFO: %s", getpid(), cFifoName);
	}

	for(;;){

		pNewClientMsg = calloc( 1, MSG_HEADER_SIZE);
		if( test_malloc_ptr( pNewClientMsg, &status )!= SUCCESS ){
			snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d: pNewClientMsg calloc failed. ", pid);
			reportErrorToClient( &workerInfo, &status, errStr );	
		}
			
		n = read(workerInfo.serverFd, pNewClientMsg, MSG_HEADER_SIZE);
		
		if( n == PIPE_EOF ){			// Check for EOF
			TRACE("doWorkerService PID # %d:  Found EOF! ", pid);
			performWorkerCleanup(&workerInfo);
			TRACE("doWorkerService PID # %d:  EXITING!!!! ",pid);
			exit(0);
		}
		if( n != MSG_HEADER_SIZE ){		// Check for Error
			snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d: pNewClientMsg Read Error. Expected %d Bytes. Read %d Bytes.",pid, (int)MSG_HEADER_SIZE, n);
			status = EPIPE;	
			reportErrorToClient( &workerInfo, &status, errStr );
		}
		else {							// Perform protocol
			
			switch( pNewClientMsg->code){
				case NEW_CLIENT:
						// Read the rest of the data as chars from the client
						pData	= calloc(1, pNewClientMsg->len);
						if( test_malloc_ptr( pData, &status )!= SUCCESS ){
							snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d - pData calloc failed. ", pid);
							reportErrorToClient( &workerInfo, &status, errStr );
						}
						
						if( (n = read(workerInfo.serverFd, pData, pNewClientMsg->len)) != pNewClientMsg->len ){
							snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d: pNewClientMsg Read Error.  Expected %d Bytes. Got %d Bytes (Client PID = %d).", (int)pid, pNewClientMsg->len, n, (int)pNewClientMsg->pid);
							status = EPIPE;	
							reportErrorToClient( &workerInfo, &status, errStr );				
						}
						setupNewClient( &workerInfo, pData );
						free (pData);
					break;
				case NEW_PROBLEM:
						setupNewProblem( pNewClientMsg, &workerInfo, &mulProblem );
					break;
				case A_MATRIX:
						// Read the rest of the data as MatrixBaseType from the client
						if( (n = read(workerInfo.serverFd, mulProblem.pM1, mulProblem.sizeM1)) != mulProblem.sizeM1 ){
							snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d: Matrix A Read Error.  Expected %d Bytes. Got %d Bytes (Client PID = %d).", (int)pid, pNewClientMsg->len, n, (int)pNewClientMsg->pid);
							status = EPIPE;	
							reportErrorToClient( &workerInfo, &status, errStr );				
						}
					break;
				case B_MATRIX:
						// Read the rest of the data as MatrixBaseType from the client
						if( (n = read(workerInfo.serverFd, mulProblem.pM2, mulProblem.sizeM2)) != pNewClientMsg->len ){
							snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d: Matrix A Read Error.  Expected %d Bytes. Got %d Bytes (Client PID = %d).", (int)pid, pNewClientMsg->len, n, (int)pNewClientMsg->pid);
							status = EPIPE;	
						}
						executeMultiply( &workerInfo, &mulProblem );
						cleanUpProblem( &workerInfo, &mulProblem );
					break;
				default:
					snprintf( errStr, MSG_STR_MAX, "doWorkerService PID # %d: Unexpected message received:  CODE = 0x%08X", pid, pNewClientMsg->code);
					status = EPROTO;	
					reportErrorToClient( &workerInfo, &status, errStr );				
			}
		}
		free(pNewClientMsg);
	}
}

// ---------------------------------------------------------------------------------------------------------
// doDaemonService
// ---------------------------------------------------------------------------------------------------------
// This routine is the long-lived daemon service
// It is responsible for reading PIDs from the well-known FIFO and spawning new worker processes when a new
// client PID is received.
// ---------------------------------------------------------------------------------------------------------
void doDaemonService(const char *serverDir){
	int 		serverFd, dummyFd;
	pid_t 		receivedPid = ERROR, childPid = ERROR, grandchildPid = ERROR ;

	umask(0);		/* So we get the permissions we want */
	if( (mkfifo(SERVER_FIFO, S_IRUSR | S_IWUSR | S_IWGRP) == ERROR) && (errno != EEXIST)) {
		fatal("Error creating well-known FIFO: %s", SERVER_FIFO);
	}
	serverFd = open(SERVER_FIFO, O_RDONLY);
	
	if(serverFd == ERROR ){
		fatal("Error opening well-known FIFO: %s", SERVER_FIFO);
	}
	
	/* Open the extra write descriptor so that we never see the end of file */
	dummyFd = open(SERVER_FIFO, O_WRONLY);
	if(dummyFd == ERROR){
		fatal("Error opening dummy FIFO.");
	}	
	
	/* Read requests and respond forever and ever amen */
	for(;;){
		if( read(serverFd, &receivedPid, sizeof(pid_t)) != sizeof(pid_t) ){
			fprintf(stderr, "PID # %d - %s :  Error reading request; Discarding.... ", getpid(), SERVER_FIFO);
			continue;
		}
		TRACE("PID # %d %s -  Got one!!!  New PID = %d", getpid(), SERVER_FIFO, receivedPid );
		switch( childPid = fork() ){
			case ERROR:
				fprintf(stderr,"SERVER PID # %d :  Error forking child: %d  ", getpid(), childPid);
				break;					
				
			case CHILD:
			
				grandchildPid = fork();
				if( grandchildPid == ERROR ){
					fatal("SERVER PID # %d :  Error forking grandchild %d (childPid = %d)  ", getpid(), grandchildPid, childPid);
				}
				else if( grandchildPid > 0 ){
					TRACE("Worker's Child Process (PID # %d) has spawned a grandchild and is now exiting.....", grandchildPid);
					exit(0);
				}
				doWorkerService(  receivedPid, serverDir );
				break;
			default:	/* PARENT */
				break;
			}
			
			/* Wait for the child process so we don't get a ZOMBIE.... */
			if( waitpid(childPid, NULL, 0) != childPid){
				fatal("SERVER PID # %d :  Error waiting on child (PID # %d) ", getpid(), childPid);
			}
	}
}



// ---------------------------------------------------------------------------------------------------------
// makeDaemon
// ---------------------------------------------------------------------------------------------------------
// Short lived process whose only job is to spawn the Daemon process and return the daemon's PID to the
// caller.
// ---------------------------------------------------------------------------------------------------------
static pid_t makeDaemon(const char *serverDir)
{
  pid_t 	pid;
  
 pid = fork();
  switch( pid ){
	  case ERROR:
	    fatal("makeDaemon - fork error ");
		break;
	  case CHILD:
		/* Become a daemon */
		setsid();
		goToServerDir(serverDir);
		umask(0);
		doDaemonService(serverDir);
		assert(0);			/* Should never exit */
		break;
	  default:	/* PARENT */
		return pid;
  }
  return ERROR; 	//should never get here
}

// ---------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
	const char *serverDir = argv[1];
	char		errString[MSG_STR_MAX];
	errno = 0;

	/* Basic error checking */
	if (argc != 2) fatal("usage: %s <server-dir>", argv[0]);
	if(mkdir(serverDir, 0777) < 0) {
		if( errno != EEXIST ){
			sprintf(errString, "Error creating directory: %s", serverDir);
			errExit(errString);
		}
	}
	
	pid_t pid = makeDaemon(serverDir);
	printf("%ld\n", (long)pid);

	return 0;
}
