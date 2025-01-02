

#ifndef _COMMON_H
#define _COMMON_H

#include "mat_base.h"
#include "trace.h"
#include "errors.h"

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

// #define 	DO_TRACE 

// -------------------------------------------------
// CONSTANTS 
// -------------------------------------------------
enum 	{ READ = 0, WRITE = 1 };
enum	{ ERROR = -1, SUCCESS = 0 };
enum 	{ FALSE = 0, TRUE = 1 };
enum 	{ SERVER = 0x0005, CLIENT = 0x000C };
enum 	{ CHILD = 0 };
enum 	{ PIPE_EOF = 0 };
enum	{ MSG_STR_MAX = 255 };
enum 	{ NEW_CLIENT   = 0xC0DE0000,		/* From client to server */
		  NEW_PROBLEM  = 0xC0DE0022,		/* From client to server */
		  A_MATRIX     = 0xC0DE000A,		/* From client to server */
		  B_MATRIX     = 0xC0DE000B,		/* From client to server */
		  SERVICE_READY= 0xC0DE0011,		/* From server to client */
		  C_MATRIX     = 0xC0DE000C,		/* From server to client */
		  SERVER_ERROR = 0xC0DE0BAD };		/* From server to client */
		  
#define		SIZEOF_MBT					( sizeof(MatrixBaseType) )
#define 	MSG_HEADER_SIZE				( sizeof(MsgHeader_T) )

// See Kerrisk text p. 911
/* Well-known name for server's FIFO */
#define SERVER_FIFO				"matmul_sv"

/* Template for building client's private FIFO name */
#define CLIENT_FIFO_TEMPLATE	"matmul.c_%ld"

/* Template for building server's private FIFO name */
#define SERVER_FIFO_TEMPLATE	"matmul.s_%ld"

/* Space required for private FIFO pathname (+20 as a generous allowance for the PID) */
#define PRIVATE_FIFO_NAME_LEN	(sizeof(CLIENT_FIFO_TEMPLATE)+20)

// -------------------------------------------------
// COMMON TYPES
// -------------------------------------------------		  
typedef struct MsgHeader_TYPE{
	int 				code;				    /* MESSAGE START MARKER */
	pid_t 				pid;					/* PID of client */
	int 				len;					/* number of bytes of data that follow this message */
	int 				n1;						/* M1 and M3 number of rows */
	int 				n2;						/* M1 columns and M2 rows */
	int 				n3;						/* M2 and M3 number of columns */
	int 				errCode;				/* Error code (if applicable) */
} MsgHeader_T;
		  		  
typedef struct MulProblem_TYPE{
	int 			  n1;
	int 			  n2;
	int 			  n3;
	int 			  sizeM1;			/* Size of M1 in bytes */
	int 			  sizeM2;			/* Size of M2 in bytes */
	int 			  sizeM3;			/* Size of M3 in bytes */	
	MatrixBaseType 	*pM1;				/* pM1, pM2, pM3 need to in contiguous memory */
	MatrixBaseType 	*pM2;
	MatrixBaseType	*pM3;
} MulProblem_T;
		  
/** Name of well-known requests FIFO in server-dir used by both clients
 *  and server.
 */
#define REQUESTS_FIFO "REQUESTS"

/* add declarations common to both server and client */
/* =============== PROTOTYPES ==================== */
int  test_malloc_ptr( void * ptr, int *pErr );
void get_private_fifo_name( int type, pid_t pid, char *pFifoNameString );
void goToServerDir( const char * serverDir );
void checkFilePath( char *pFilePath );
void myOutMatrix(FILE *out, int nRows, int nCols, CONST MatrixBaseType M[nRows][nCols], const char *label);
void errExit( char * errString );
#endif //ifndef _COMMON_H
