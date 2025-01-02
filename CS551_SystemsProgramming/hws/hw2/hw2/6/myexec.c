#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "get_ids.h"

int main( int argc, char *argv[] ){
	
	puts("A.  Initial values.");
	print_ids();


	// execlp("/home/excelsior/Desktop/hw2/6/e/excelsior", argv[1], (char *) NULL);

	puts("B.  Successfully exec's a setuid program owned by UID 2000");
	execlp("/home/excelsior/Desktop/hw2/6/t/tesla", argv[1], (char *) NULL);
	print_ids();

	return 0;
}