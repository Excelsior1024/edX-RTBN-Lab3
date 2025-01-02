#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "get_ids.h"

int main( int argc, char *argv[] ){
	
	puts("A.  Initial values.");
	print_ids();


	int fd = open("/home/excelsior/Desktop/hw2/10c/dir1/melly", O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH); 
	FILE * fp = fdopen( fd, "w" );
	file_print_ids(fp);
	print_ids();

	return 0;
}