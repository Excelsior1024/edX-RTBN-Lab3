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


#define		MY_LINE_MAX		(80)
#define 	N_PROCS			(4)
#define 	READ			(0)
#define 	WRITE			(1)
#define		CHILD			(0)
#define		ERROR			(-1)
#define 	SUCCESS 		(0)
#define 	DEBUG 			(0)
#define		CHILD			(0)

typedef int filedes_TYPE[2];

filedes_TYPE	pipeArray[N_PROCS];

int main( int argc, char *argv[]){

pid_t	pid;
char	errString[MY_LINE_MAX], inputline[MY_LINE_MAX], outputline[MY_LINE_MAX];
int		i, j, writePipeNum, readPipeNum, n1, n2;

	for( i=0; i<N_PROCS; i++){
		if( (pipe( pipeArray[i])) < 0 ){
					sprintf(errString, "Error creating pipeArray - pipe[%d]\n",i);
					perror(errString);				
		}
	}


	for( i=0; i<N_PROCS; i++){
		errno = 0;
		pid = fork();
		switch( pid ){
			case(ERROR):
				fprintf(stderr, "Fork %d Error:  %s\n", i, strerror(errno) );
				break;
			case(CHILD):
				writePipeNum = i;	// current process ID
				readPipeNum = (i + N_PROCS - 1)%N_PROCS;

				printf("\nChild # %d PID %d - Write Pipe = %d  Read Pipe = %d\n", i, getpid(), writePipeNum, readPipeNum);

				if( close(pipeArray[writePipeNum][READ]) <= ERROR ){ 
					sprintf(errString, "Error closing pipeArray[%d][WRITE]\n",writePipeNum);
					perror(errString);
				}
				if( close(pipeArray[readPipeNum][WRITE]) <= ERROR ){ 
					sprintf(errString, "Error closing pipeArray[%d][WRITE]\n",writePipeNum);
					perror(errString);
				}

				sprintf(outputline, "\nChild # %d PID %d - Writing to pipe[%d]...... ABCD ", i, getpid(), writePipeNum);
				n1 = strlen(outputline);
				if( write(pipeArray[writePipeNum][WRITE],outputline,n1) != n1 )	{
					fprintf(stderr, "Error writing to pipeArray[%d][WRITE]\n",writePipeNum);
				}
				if( n1 < 0){ 
				sprintf(errString, "Child # %d PID %d - Error writing to pipeArray[%d][WRITE]\n",i, getpid(), readPipeNum);
				perror(errString);
				}

				while( (n2 = read(pipeArray[readPipeNum][READ], inputline, LINE_MAX)) > 0) {
					fprintf(stderr, "Child # %d PID %d - Reading from pipe[%d]:  ", i, getpid(), readPipeNum);
					for(j=0; j<n2; j++ ){
						printf("%c-",inputline[j]);
					}
					puts("\n");
				}
				if( n2 < 0){ 
					sprintf(errString, "Child # %d PID %d - Error reading from pipeArray[%d][WRITE]\n",i, getpid(), readPipeNum);
					perror(errString);
				}
				break;
				
			default:   /* PARENT */
				// fprintf(stderr, "*** PARENT PID# %d - fork(): fork() returned PID %d  (myParentPID # %d) \n", getpid(), (int) *pPid, getppid());
				break;
		}
	}


}