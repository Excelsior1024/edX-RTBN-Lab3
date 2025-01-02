#include "get_ids.h"


void print_ids(void){
	
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;

	puts("USER IDs");
	if( (getresuid( &ruid, &euid, &suid)) == 0){
		printf("real uid  = %d", ruid);
		printf("\neffective uid = %d", euid);
		printf("\nsaved set uid = %d\n\n", suid);
	}
	else {
		perror("getresuid() failed.");
	}

	puts("GROUP IDs");
	if( (getresgid( &rgid, &egid, &sgid)) == 0){
		printf("real gid  = %d", rgid);
		printf("\neffective gid = %d", egid);
		printf("\nsaved set gid = %d\n\n", sgid);
	}
	else {
		perror("getresgid() failed.");
	}


}


void file_print_ids(FILE * fp){
	
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;


	fprintf(fp, "USER IDs");
	if( (getresuid( &ruid, &euid, &suid)) == 0){
		fprintf(fp, "real uid  = %d", ruid);
		fprintf(fp, "\neffective uid = %d", euid);
		fprintf(fp, "\nsaved set uid = %d\n\n", suid);
	}
	else {
		perror("getresuid() failed.");
	}

	fprintf(fp, "GROUP IDs");
	if( (getresgid( &rgid, &egid, &sgid)) == 0){
		fprintf(fp, "real gid  = %d", rgid);
		fprintf(fp, "\neffective gid = %d", egid);
		fprintf(fp, "\nsaved set gid = %d\n\n", sgid);
	}
	else {
		perror("getresgid() failed.");
	}

}
