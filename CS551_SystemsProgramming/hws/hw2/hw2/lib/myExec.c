#include <stdio.h>
#include <errno.h>

void print_ids(void){
	
	uid_t *pRuid, *pEuid, *pSuid;

	if( (getresuid( pRuid, pEuid, pSuid)) == 0){
		printf("\nreal id  = %d", *pRuid);
		printf("\neffective id = %d", *pEuid);
		printf("\nsaved set uid = %d", *pSuid);
	}
	else {
		perror("getresuid failed.  [%s]");
		
	}
}


int main( int argc, char argv[] ){
	
	print_ids();


}