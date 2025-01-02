#include "get_ids.h"

void print_ids(void){
	
	uid_t ruid, euid, suid;

	if( (getresuid( &ruid, &euid, &suid)) == 0){
		printf("real id  = %d", ruid);
		printf("\neffective id = %d", euid);
		printf("\nsaved set uid = %d\n\n", suid);
	}
	else {
		perror("getresuid() failed.");
	}
}


void file_print_ids( FILE * fp){
	
	uid_t ruid, euid, suid;

	if( (getresuid( &ruid, &euid, &suid)) == 0){
		fprintf(fp, "real id  = %d", ruid);
		fprintf(fp, "\neffective id = %d", euid);
		fprintf(fp, "\nsaved set uid = %d\n\n", suid);
	}
	else {
		perror("getresuid() failed.");
	}

}
