#ifndef _GET_IDS_H
#define _GET_IDS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

void print_ids(void);
void file_print_ids(FILE * fp);
	
#endif