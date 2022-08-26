#include <stdio.h>
#include "udp.h"
#include "mfs.c"


int
main(int argc, char *argv[])
{
	printf("Starting Client\n");

	printf("Sizes %u %u %u\n", (unsigned int)sizeof(MFS_CR_t), (unsigned int)sizeof(MFS_Imap_t), (unsigned int)sizeof(MFS_Inode_t));

	MFS_Init("localhost", 3004);
	int rc = -1;
	//	rc = MFS_Creat(0, MFS_DIRECTORY, "testdir");

	int i = 0;
	for(i=0; i<1; i++) {
	  int aInt = i;
	  char str[15];
	  sprintf(str, "%d", aInt);
	  //	  int rc = MFS_Creat(0, MFS_REGULAR_FILE, str);
	  rc = MFS_Creat(0, MFS_DIRECTORY, str);
	  if(rc < 0){
	    printf("Failed at Creat 0_%d\n",i);
	    exit(0);
	  }
	  /* rc = MFS_Lookup(0, str); */
	  /* assert(rc == 1); */

	  rc = MFS_Lookup(1, ".");
	  printf("\n Under created dir, \".\"'s inum is: %d", rc);
	  rc = MFS_Lookup(1, "..");
	  printf("\n Under created dir, \"..\"'s inum is: %d", rc);

	  //	  printf("Passed Creat 0_%d\n", i);
	  //	  char str1[15];
	  int inum = MFS_Lookup(0, str);
	  //	  if(rc < 0){
	  printf("File name %s has inum of %d\n", str,inum);
	  //	  exit(0);
	  //	  }

	}


	rc = MFS_Creat(0, MFS_REGULAR_FILE, "test");
	if(rc < 0){
		printf("Failed at Creat 1\n");
		exit(0);
	}
	printf("Passed Creat 1\n");

	rc = MFS_Creat(0, MFS_DIRECTORY, "test_dir");
	if(rc < 0){
		printf("Failed at Creat 2\n");
	}
	printf("Passed Creat 2\n");

	int inum = MFS_Lookup(0, "test");
	if(inum < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup 1\n");

	int inum2 = MFS_Lookup(0, "test_dir");
	if(inum2 < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup 1\n");


	rc = MFS_Creat(inum2, MFS_REGULAR_FILE, "test");
	if(rc < 0){
		printf("Failed at Creat 3\n");
		exit(0);
	}
	printf("Passed Creat 3\n");

 	inum2 = MFS_Lookup(inum2, "test");
	if(inum2 < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup\n");


	char* tx_buffer = malloc(MFS_BLOCK_SIZE);
	char* tx_buffer2 = malloc(MFS_BLOCK_SIZE);
	char* rx_buffer = malloc(MFS_BLOCK_SIZE);
	strcpy(tx_buffer, "This is just a test!");
	strcpy(tx_buffer2, "Is this a test?");
	
	//	rc = MFS_Write(inum, tx_buffer, 2);
	rc = MFS_Write(inum, tx_buffer, 1);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	MFS_Read(inum, rx_buffer, 1);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	printf("Passed Write\n");

	if(strcmp(rx_buffer, tx_buffer) != 0){
		printf("%s - %s\n", rx_buffer, tx_buffer);
		printf("Failed at Write - Strings does not match\n");
		exit(0);
	}
	printf("Passed Write\n");
	rc = MFS_Write(inum, tx_buffer2, 2);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	printf("Passed Write\n");
	MFS_Read(inum, rx_buffer, 2);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	printf("Passed Write\n");
	if(strcmp(rx_buffer, tx_buffer2) != 0){
		printf("%s - %s\n", rx_buffer, tx_buffer2);
		printf("Failed at Write - Strings does not match\n");
		exit(0);
	}
	printf("Passed Write\n");
	//	exit(0);

	rc = MFS_Unlink(0, "test");
	if(rc == -1){
		printf("Failed at Unlink 1\n");
		exit(0);
	}
	printf("Passed Unlink\n");

	inum = MFS_Lookup(0, "test");
	if(inum >= 0){
		printf("Failed at Lookup Ghost\n");
		exit(0);
	}
	printf("Passed Lookup Ghost\n");
	
	rc = MFS_Unlink(0, "test");
	if(rc == -1){
		printf("failed at unlink 2\n");
		exit(0);
	}
	printf("Passed unlink 2\n");

	rc = MFS_Unlink(0, "test_dir");
	if(rc != -1){
		printf("failed at unlink 3\n");
		exit(0);
	}
	printf("Passed unlink 3\n");

	inum2 = MFS_Lookup(0, "test_dir");
	if(inum2 < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup\n");

	rc = MFS_Unlink(inum2, "test");
	if(rc == -1){
		printf("failed at unlink 4\n");
		exit(0);
	}
	printf("Passed unlink 4\n");
	
	inum2 = MFS_Lookup(inum2, "test");
	if(inum2 >= 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup\n");

	rc = MFS_Unlink(0, "test_dir");
	if(rc == -1){
		printf("failed at unlink 5\n");
		exit(0);
	}
	printf("Passed unlink 5\n");

	printf("\nall test passed!\n");
	return 0;
}
