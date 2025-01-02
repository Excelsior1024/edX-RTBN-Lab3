#include <stdio.h>
#include <errno.h>
#include "get_ids.h"

int main( int argc, char **argv){

	printf("\n\nTESLA!!!!!\n\n");
	print_ids();

	puts("C.  setuid program calling  setuid(2000)");
	if( (setuid(2000)) == 0  ){
		print_ids();
	}
	else{
		perror("setuid(2000) failed.");
	}

	puts("D.  setuid program calling seteuid(1000)");
	if ( (seteuid(1000)) == 0){
		print_ids();
	}
	else{
		perror("seteuid(1000) failed.");
	}

	puts("E.  setuid program calling setreuid(1000, 2000)");
	if( (setreuid(1000,2000)) == 0){
		print_ids();
	}
	else{
		perror("setreuid(1000, 2000).");
	}

	puts("F.  setuid program calling setreuid(1000,1000)");
	if( (setreuid(1000,1000)) == 0 ){
		print_ids();
	}
	else{
		perror("setreuid(1000,2000) failed.");
	}

}
