#include "common.h"

//#define DO_TRACE 1
#include "trace.h"

/* implement definitions common to both server and client */

// --------------------------------------------------------------
// test_malloc_ptr
// --------------------------------------------------------------
int test_malloc_ptr( void * ptr, int *pErr ){
	
	if (ptr==NULL){
		*pErr = ENOMEM;
		return ERROR;
	}
	return SUCCESS;
}

// --------------------------------------------------------------
// errExit
// --------------------------------------------------------------
void errExit( char * errString ){
	fprintf(stderr,"%s [%s]\n", errString, strerror(errno));
	exit(ERROR);
}
	
	
// --------------------------------------------------------------
// get_private_fifo_name
// --------------------------------------------------------------
void get_private_fifo_name( int type, pid_t pid, char *pFifoNameString ){

	char 	errorString[MSG_STR_MAX] = {0};
			
	if( type != CLIENT && type != SERVER ){
		snprintf(errorString, MSG_STR_MAX, "Common:get_private_fifo_name - Incorrect value supplied for type (0x%08X)\n", type);
		errExit(errorString);
	}

	if( pid <= 0 ){
		snprintf(errorString, MSG_STR_MAX, "Common:get_private_fifo_name - Incorrect value supplied for pid (0x%d)\n", pid);
		errExit(errorString);
	}
	
	( type == CLIENT ?  
		snprintf(pFifoNameString, PRIVATE_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)pid) : 
		snprintf(pFifoNameString, PRIVATE_FIFO_NAME_LEN, SERVER_FIFO_TEMPLATE, (long)pid) );
		
}	


// --------------------------------------------------------------
// goToServerDir
// --------------------------------------------------------------
void goToServerDir( const char * serverDir ){
	struct stat  	   _stat = {0};
	char 			   errString[MSG_STR_MAX] = {0};
	char 			   *pCwd = NULL;
	char 			   fullDirPath[PATH_MAX] = {0};
	pid_t 			   pid = getpid();
	char 			   slash[] = {'/', '\0'};
	
	pCwd = get_current_dir_name();
	
	// change relative path to absolute
	if( serverDir[0] != '/'){
		strcpy( fullDirPath, pCwd );
		strcat( fullDirPath, slash );
		strcat( fullDirPath, serverDir );
		TRACE("goToServerDir pid:  SERVER DIR = %s", fullDirPath);
	}
	
	//TRACE("common:goToServerDir PID # %d: serverDir = %s", getpid(), serverDir);
	//Verify that the server directory exists and change to that directory
	if( (stat( fullDirPath, &_stat) != SUCCESS) && (S_ISDIR(_stat.st_mode) != TRUE )){
			snprintf(errString, MSG_STR_MAX, "Common:goToServerDir PID # %d: server directory %s does not exist ", pid, fullDirPath );
			errExit(errString);		
	}
	chdir(serverDir);		

	// Must free the memory allocated by get_current_dir_name()
	if( pCwd != NULL ){ free(pCwd); }

	// Check where we are...
	pCwd = get_current_dir_name();
	TRACE("goToServerDir pid:  CWD = %s", pCwd);
		
	// Must free the memory allocated by get_current_dir_name()
	if( pCwd != NULL ){ free(pCwd); }
}

// --------------------------------------------------------------
// checkFilePath - add final / if missing from file path
// --------------------------------------------------------------
void checkFilePath( char *pFilePath ){
	int len = strlen( pFilePath );		
	if( pFilePath[ len-1 ] != '/' ) {
		// TRACE("\npFilePath before :  %s\n", pFilePath );
		pFilePath[ len  ] = '/';
		// TRACE("\npFilePath after :  %s\n", pFilePath );
	} 		
}

// --------------------------------------------------------------
// myOutMatrix - copied from outMatrix() in client_main.c because
// I wanted to call it from the server.
// --------------------------------------------------------------
void myOutMatrix(FILE *out, int nRows, int nCols, CONST MatrixBaseType M[nRows][nCols], const char *label)
{
#ifdef 	DO_TRACE
  fprintf(out, "%s \n", label);
  for (int i = 0; i < nRows; i++) {
    for (int j = 0; j < nCols; j++) {
      fprintf(out, "%8d", M[i][j]);
    }
    fprintf(out, "\n");
  }
#endif  
}

