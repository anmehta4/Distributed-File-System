#include <stdio.h>
#include "mfs.h"

#define INODES_IN_MAP	100

typedef struct inode {
	int isFile; //0 if directory 1 if file
	char* addr_data_inode;
	int num_of_ref;
	int inum;
} inode;

typedef struct inode_map {
	inode* inodes[100];
} inode_map;

inode_map* cp_region[(MFS_BLOCK_SIZE/INODES_IN_MAP) + 1]; //4096 Total Inodes

int MFS_Init(char *hostname, int port) {
	return 0;
}

int MFS_Lookup(int pinum, char *name) {
	return 0;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	return 0;
}

int MFS_Write(int inum, char *buffer, int block) {
	return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
	return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
	return 0;
}

int MFS_Unlink(int pinum, char *name) {
	return 0;
}

int MFS_Shutdown() {
	return 0;
}


