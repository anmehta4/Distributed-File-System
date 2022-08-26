/* gw: this version actuallly works I think it is woking fine */
/* it seems there is one more error about the dir stat/ or big dir I could not be sure about i t */
/* gw: maybe need to load all imap piece into mem? */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "mfs.h"		/* gw: chg */
#include "udp.h"



int fd = -1;
MFS_CR_t* p_cr = NULL;

int server_init(int , char* );
int server_lookup(int, char* );
int server_stat(int , MFS_Stat_t* );
int server_write(int , char* , int );
int server_read(int , char* , int );
int server_creat(int , int , char* );
int server_unlink(int , char* );
int server_shutdown();
int print_block(int, int);



int server_lookup(int pinum, char*name) {

        printf("Server Lookup initiated\n");

        if(pinum<0 || pinum>=TOTAL_NUM_INODES) {
                return -1;
        }

	MFS_Imap_t imap;
        MFS_Inode_t inode;

        int imap_num = pinum / IMAP_PIECE_SIZE; //Which imap
        int fmap =  p_cr->imap[imap_num];
        if(fmap  == -1) {
               return -1;
        }

        lseek(fd, fmap, SEEK_SET);
        read(fd, &imap, sizeof(MFS_Imap_t));

        int inode_num = pinum % IMAP_PIECE_SIZE; //Which inode within map
        int finode = imap.inodes[inode_num];
        if(finode == -1) {
                return -1;
        }
        lseek(fd, finode, SEEK_SET);
        read(fd, &inode, sizeof(MFS_Inode_t));

        
        char data_block[MFS_BLOCK_SIZE];
        MFS_DirDataBlock_t* dir_data = (MFS_DirDataBlock_t*) data_block;
        MFS_DirEnt_t* entry;

        for(int i=0; i<NUM_INODE_POINTERS; i++) {
                int block_num = inode.data[i];
                if(block_num == -1) {
                        continue;
                }

                lseek(fd, block_num, SEEK_SET);
                read(fd, &data_block, MFS_BLOCK_SIZE);

                for(int j=0; j<NUM_DIR_ENTRIES; j++) {
                        entry = &dir_data->entries[j];
                        if(strncmp(entry->name, name, LEN_NAME)==0) {
                               return entry->inum;
                        }
                }
        }
        return -1;
}

int server_stat(int inum, MFS_Stat_t* m) {

        printf("Server Stat initiated\n");

        if(inum<0 || inum>=TOTAL_NUM_INODES) {
                return -1;
        }

        MFS_Imap_t imap;
        MFS_Inode_t inode;

        int imap_num = inum / IMAP_PIECE_SIZE; //Which imap
        int fmap =  p_cr->imap[imap_num];
        if(fmap  == -1) {
               return -1;
        }

        lseek(fd, fmap, SEEK_SET);
        read(fd, &imap, sizeof(MFS_Imap_t));

        int inode_num = inum % IMAP_PIECE_SIZE; //Which inode within map
        int finode = imap.inodes[inode_num];
        if(finode == -1) {
                return -1;
        }
        lseek(fd, finode, SEEK_SET);
        read(fd, &inode, sizeof(MFS_Inode_t));

        m->size = inode.size;
        m->type = inode.type;

        return 0;
}

int server_write(int inum, char* buffer, int block) {

        printf("Server Write initiated\n");
        MFS_Inode_t inode;
        MFS_Inode_t inode_new;
        MFS_Imap_t imap;
        MFS_Imap_t imap_new;
        int offset;

        if(inum<0 || inum>=TOTAL_NUM_INODES || block<0 || block >= NUM_INODE_POINTERS) {
                return -1;
        }

        /*Create a new buffer*/
        char write_buffer[MFS_BLOCK_SIZE];
	char* copy = buffer;
        for(int i = 0; i<MFS_BLOCK_SIZE; i++) {
		if(copy != NULL) {
                	write_buffer[i] = *copy; copy++;
		} else {
			write_buffer[i] = '\0';
		}
        }

        /*Obtain imap if there is one*/
        offset = p_cr->end_log;
        int imap_num = inum / IMAP_PIECE_SIZE; //Which imap
        int fmap =  p_cr->imap[imap_num];
        if(fmap  != -1) {
                lseek(fd, fmap, SEEK_SET);
                read(fd, &imap, sizeof(MFS_Imap_t));
        }

         /*Obtain inode if there is one*/
        int inode_num = inum % IMAP_PIECE_SIZE; //Which inode within map
        int finode = imap.inodes[inode_num];
        if(finode != -1 && fmap != -1) {
                lseek(fd, finode, SEEK_SET);
                read(fd, &inode, sizeof(MFS_Inode_t));
        
        	if(inode.type != MFS_REGULAR_FILE) {
                	return -1;
		}
        }

        /*Obtain data block if there is one*/
        int fblock = inode.data[block];
        if(fblock != -1 && finode!= -1 && fmap!= -1) {
                offset = fblock;
        }

        /*Write the the new buffer created*/
        p_cr->end_log += MFS_BLOCK_SIZE;
        lseek(fd, offset, SEEK_SET);
        write(fd, write_buffer, MFS_BLOCK_SIZE);

        /*Create new inode based on existing or new inode req*/
        if(finode != -1) {
                inode_new.size = (block+1)*MFS_BLOCK_SIZE;
                inode_new.type = inode.type;
        } else {
                inode_new.size = 0;
                inode_new.type = MFS_REGULAR_FILE;
        }
        for(int i=0; i<NUM_INODE_POINTERS; i++) {
		if(finode != -1) {
                	inode_new.data[i] = inode.data[i];
		} else {
			inode_new.data[i] = -1;
		}
        }
        inode_new.data[block] = offset;
        offset = p_cr->end_log;

 	/*Write new inode to checkpoint*/
        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, &inode_new, sizeof(MFS_Inode_t));
        p_cr->end_log += sizeof(MFS_Inode_t);

        /*Create new imap based on existing or new imap req*/
        for(int i=0; i< IMAP_PIECE_SIZE; i++) {
                if(fmap != -1) {
                        imap_new.inodes[i] = imap.inodes[i];
                } else {
                        imap_new.inodes[i] = -1;
                }
        }
        imap_new.inodes[inode_num] = offset;
        offset = p_cr->end_log;

        /*Write new imap to checkpoint*/
        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, &imap_new, sizeof(MFS_Imap_t));
        p_cr->imap[imap_num] = offset;
	p_cr->end_log += sizeof(MFS_Imap_t);

        /*Update the checkpoint*/
        lseek(fd, 0, SEEK_SET);
        write(fd, p_cr, sizeof(MFS_CR_t));
        fsync(fd);

        return 0;
}


int server_read(int inum, char* buffer, int block) {

        printf("Server Read initiated\n");


        if(inum<0 || inum>=TOTAL_NUM_INODES || block<0 || block >= NUM_INODE_POINTERS)  {
                return -1;
        }

        MFS_Imap_t imap;
        MFS_Inode_t inode;

        int imap_num = inum / IMAP_PIECE_SIZE; //Which imap
        int fmap =  p_cr->imap[imap_num];
        if(fmap  == -1) {
               return -1;
        }

        lseek(fd, fmap, SEEK_SET);
        read(fd, &imap, sizeof(MFS_Imap_t));

        int inode_num = inum % IMAP_PIECE_SIZE; //Which inode within map
        int finode = imap.inodes[inode_num];
        if(finode == -1) {
                return -1;
        }
        lseek(fd, finode, SEEK_SET);
        read(fd, &inode, sizeof(MFS_Inode_t));

        int fblock = inode.data[block];
        lseek(fd, fblock, SEEK_SET);
        read(fd, buffer, MFS_BLOCK_SIZE);

        return 0;
}

int server_creat(int pinum, int type, char* name){
    
    int i=0, j=0, k=0, l=0;
    int fmap = -1, finode = -1, fblock = -1;

    MFS_Inode_t inode;
    MFS_Imap_t imap;

    int len_name = 0;
    for(i=0; name[i] != '\0'; i++, len_name ++);

    if(pinum < 0 || pinum >= TOTAL_NUM_INODES || len_name > LEN_NAME) {
        return -1;
    }

    if(server_lookup(pinum, name) != -1) {
        return 0;
    }

    MFS_Imap_t imap_parent;
    int imap_num = pinum / IMAP_PIECE_SIZE; 
    fmap =  p_cr->imap[imap_num];
    if(fmap == -1){
        return -1;
    }

    lseek(fd, fmap, SEEK_SET);
    read(fd, &imap_parent, sizeof(MFS_Imap_t));

    MFS_Inode_t inode_parent;
    int inode_num = pinum % IMAP_PIECE_SIZE; 
    finode = imap_parent.inodes[inode_num]; 
    if(finode == -1) {
        perror(/*"server_creat: invalid pinum_3"*/ name);
        return -1;
    }
    lseek(fd, finode, SEEK_SET);
    read(fd, &inode_parent, sizeof(MFS_Inode_t));

    if(inode_parent.type != MFS_DIRECTORY) {
        return -1;
    }


    int inum_free = -1;
    int found_inum = 0;

    for(i=0; i<NUM_IMAP; i++) {

        fmap =  p_cr->imap[i];
        if(fmap != -1) {
            MFS_Imap_t imap_parent_temp;	     
            lseek(fd, fmap, SEEK_SET);
            read(fd, &imap_parent_temp, sizeof(MFS_Imap_t));

            for(j=0; j<IMAP_PIECE_SIZE; j++) {
                finode = imap_parent_temp.inodes[j]; 
                if(finode == -1) {
                    inum_free = i*IMAP_PIECE_SIZE + j;
                    found_inum = 1;
                    break;
                }
            }
        }
        else {

            MFS_Imap_t imap_new_temp;
            for(j = 0; j< IMAP_PIECE_SIZE; j++) {
                imap_new_temp.inodes[j] = -1 ;
            } 

            lseek(fd, p_cr->end_log, SEEK_SET);
            write(fd, &imap_new_temp, sizeof(MFS_Imap_t));

            p_cr->imap[i] = p_cr->end_log;	
            p_cr->end_log += sizeof(MFS_Imap_t);

            lseek(fd, 0, SEEK_SET);
            write(fd, p_cr, sizeof(MFS_CR_t));
            fsync(fd);

            for(j=0; j<IMAP_PIECE_SIZE; j++) {
                finode = imap_new_temp.inodes[j]; 
                if(finode == -1) {
                    inum_free = i*IMAP_PIECE_SIZE + j;
                    found_inum = 1;
                    break;
                }
            }
        }
        if (found_inum) {
            break;
        }
    }

    if(inum_free == -1 || inum_free > TOTAL_NUM_INODES - 1) { 
        return -1;
    }

    MFS_Inode_t* inode_addr = &inode_parent;
    MFS_DirDataBlock_t* dir_buf = NULL;
    char data_buf[MFS_BLOCK_SIZE];
    int found_dir = 0;
    int parent_block = 0;


    for(i=0; i< NUM_INODE_POINTERS; i++) {

        fblock = inode_addr->data[i];	
        parent_block = i;
        if(fblock == -1) {
            MFS_DirDataBlock_t* p_dir = (MFS_DirDataBlock_t*) data_buf;
            for(i=0; i< NUM_DIR_ENTRIES; i++){
                strcpy(p_dir->entries[i].name, "\0");
                p_dir->entries[i].inum = -1;
            }

            lseek(fd, p_cr->end_log, SEEK_SET);
            write(fd, p_dir, sizeof(MFS_DirDataBlock_t));
            fblock = p_cr->end_log;

            MFS_Inode_t inode_dir_new;
            inode_dir_new.size = inode_parent.size; 
            inode_dir_new.type = MFS_DIRECTORY;
            for (i = 0; i < 14; i++) {
                inode_dir_new.data[i] = inode_parent.data[i];
            }  
            inode_dir_new.data[parent_block] = p_cr->end_log;
            inode_addr = &inode_dir_new;	

            p_cr->end_log += MFS_BLOCK_SIZE;
            lseek(fd, p_cr->end_log, SEEK_SET);
            write(fd, &inode_dir_new, sizeof(MFS_Inode_t));	

            MFS_Imap_t imap_dir_new;
            for(i = 0; i< IMAP_PIECE_SIZE; i++) {
                imap_dir_new.inodes[i] = imap_parent.inodes[i]; 
            } 
            imap_dir_new.inodes[inode_num] = p_cr->end_log;

            p_cr->end_log += sizeof(MFS_Inode_t);
            lseek(fd, p_cr->end_log, SEEK_SET);
            write(fd, &imap_dir_new, sizeof(MFS_Imap_t));	

            p_cr->imap[imap_num] = p_cr->end_log;
            p_cr->end_log += sizeof(MFS_Imap_t);
            lseek(fd, 0, SEEK_SET);
            write(fd, p_cr, sizeof(MFS_CR_t));
            fsync(fd);
        }

        lseek(fd, fblock, SEEK_SET);
        read(fd, data_buf, MFS_BLOCK_SIZE);
        
        dir_buf = (MFS_DirDataBlock_t*)data_buf;
        for(j=0; j<NUM_DIR_ENTRIES; j++) {
            MFS_DirEnt_t* dir = &dir_buf->entries[j];
            if(dir->inum == -1) { 
                dir->inum = inum_free;
                strcpy(dir->name, name);
                found_dir = 1;
                break;
            }
        }

        if(found_dir) {
            break;
        }
    }

    if(found_dir == 0) {
	    return -1;
    }

    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, dir_buf, sizeof(MFS_DirDataBlock_t)); 

    MFS_Inode_t inode_parent_new;
    inode_parent_new.size = inode_addr->size; 
    inode_parent_new.type = MFS_DIRECTORY;

    for (i = 0; i < 14; i++) {
        inode_parent_new.data[i] = inode_addr->data[i]; 
    }
    inode_parent_new.data[parent_block] = p_cr->end_log;

    p_cr->end_log += MFS_BLOCK_SIZE;
    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, &inode_parent_new, sizeof(MFS_Inode_t));	


    MFS_Imap_t imap_parent_new;
    for(i = 0; i< IMAP_PIECE_SIZE; i++) {
        imap_parent_new.inodes[i] = imap_parent.inodes[i];
    } 
    imap_parent_new.inodes[inode_num] = p_cr->end_log;

    p_cr->end_log += sizeof(MFS_Inode_t);
    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, &imap_parent_new, sizeof(MFS_Imap_t));	

    p_cr->imap[imap_num] = p_cr->end_log;
    p_cr->end_log += sizeof(MFS_Imap_t);
    lseek(fd, 0, SEEK_SET);
    write(fd, p_cr, sizeof(MFS_CR_t));

    fsync(fd);



    char write_buffer[ MFS_BLOCK_SIZE ];
    for(i=0; i<MFS_BLOCK_SIZE; i++) {
        write_buffer[i] = '\0';
    }

    int inum = inum_free;
    fmap = -1, finode = -1, fblock = -1;

    if(type == MFS_DIRECTORY) {
        MFS_DirDataBlock_t* dir = (MFS_DirDataBlock_t*) write_buffer;
        for(i=0; i< NUM_DIR_ENTRIES; i++){
            strcpy(dir->entries[i].name, "\0");
            dir->entries[i].inum = -1;
        }
        strcpy(dir->entries[0].name, ".\0");
        dir->entries[0].inum = inum; 
        strcpy(dir->entries[1].name, "..\0");
        dir->entries[1].inum = pinum; 

        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, write_buffer, MFS_BLOCK_SIZE);
    }


    imap_num = inum / IMAP_PIECE_SIZE;
    fmap =  p_cr->imap[imap_num];

    if(fmap != -1){
        inode_num = inum % IMAP_PIECE_SIZE; 
        lseek(fd, fmap, SEEK_SET);
        read(fd, &imap, sizeof(MFS_Imap_t));
        finode = imap.inodes[inode_num]; 
    }

    MFS_Inode_t inode_new;
    inode_new.size = 0;	
    inode_new.type = type;	
    for (i = 0; i < 14; i++) {
        inode_new.data[i] = -1; 
    }
    if(type == MFS_DIRECTORY) {
        inode_new.data[0] = p_cr->end_log;
    }

    p_cr->end_log += MFS_BLOCK_SIZE;
    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, &inode_new, sizeof(MFS_Inode_t));

    MFS_Imap_t imap_new;
    if(fmap != -1) {
        for(i = 0; i< IMAP_PIECE_SIZE; i++) { 
            imap_new.inodes[i] = imap.inodes[i]; 
        }
        imap_new.inodes[inode_num] = p_cr->end_log; 	/* finode_new */
    }
    else {
        for(i = 0; i< IMAP_PIECE_SIZE; i++) { 
            imap_new.inodes[i] = -1 ; 
        }
        imap_new.inodes[inode_num] = p_cr->end_log; 	/* finode_new */
    }

    p_cr->end_log += sizeof(MFS_Inode_t);
    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, &imap_new, sizeof(MFS_Imap_t));

    p_cr->imap[imap_num] = p_cr->end_log;
    p_cr->end_log += sizeof(MFS_Imap_t);
    lseek(fd, 0, SEEK_SET);
    write(fd, p_cr, sizeof(MFS_CR_t));

    fsync(fd);
    return 0;
}



int server_unlink(int pinum, char* name){

    MFS_Inode_t inode;
    MFS_Imap_t imap;

    if(pinum < 0 || pinum >= TOTAL_NUM_INODES) {
        return -1;
    }

    int inum = server_lookup(pinum, name);
    if(inum == -1) {
        return 0;
    }

    int imap_num =inum / IMAP_PIECE_SIZE;
    if(p_cr->imap[imap_num] == -1){
        return -1;
    }
    int fmap =  p_cr->imap[imap_num];

    int inode_num = inum % IMAP_PIECE_SIZE;

    lseek(fd, fmap, SEEK_SET);
    read(fd, &imap, sizeof(MFS_Imap_t));
    int finode = imap.inodes[inode_num];
    if(finode == -1) {
        return -1;
    }

    lseek(fd, finode, SEEK_SET);
    read(fd, &inode, sizeof(MFS_Inode_t));


    if(inode.type == MFS_DIRECTORY) {
            char data_buf[MFS_BLOCK_SIZE];
            for(int i=0; i< NUM_INODE_POINTERS; i++) {
                    int fblock = inode.data[i];

                    if(fblock == -1) {
                            continue;
                    }
            lseek(fd, fblock, SEEK_SET);
            read(fd, data_buf, MFS_BLOCK_SIZE);

            MFS_DirDataBlock_t* dir_buf = (MFS_DirDataBlock_t*)data_buf;
            for(int j=0; j<NUM_DIR_ENTRIES; j++) {
                MFS_DirEnt_t* dir = &dir_buf->entries[j];
                if(!(dir->inum == pinum || dir->inum == inum || dir->inum == -1 )) {
                    perror("server_unlink: dir not empty_4");
                    return -1;
                }
            }
        }
    }

    MFS_Imap_t imap_new;
    for(int i = 0; i< IMAP_PIECE_SIZE; i++) {
        imap_new.inodes[i] = imap.inodes[i];
    }
    imap_new.inodes[inode_num] = -1;

    int is_imap_new_empty = 1;
    for(int i = 0; i< IMAP_PIECE_SIZE; i++) {
        if(imap_new.inodes[i] != -1){
        is_imap_new_empty = 0;
        break;
        }
    }

    if(is_imap_new_empty) {
        p_cr->imap[imap_num] = -1;
        lseek(fd, 0, SEEK_SET);
        write(fd, p_cr, sizeof(MFS_CR_t));

        fsync(fd);
    }
    else {
        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, &imap_new, sizeof(MFS_Imap_t));

        p_cr->imap[imap_num] = p_cr->end_log;
        p_cr->end_log += sizeof(MFS_Imap_t);
        lseek(fd, 0, SEEK_SET);
        write(fd, p_cr, sizeof(MFS_CR_t));

        fsync(fd);
    }

    imap_num = pinum / IMAP_PIECE_SIZE;
    int fmap_par =  p_cr->imap[imap_num];
    if(fmap_par == -1){
        return -1;
    }

    inode_num =pinum % IMAP_PIECE_SIZE;
    MFS_Imap_t mp_par;
    lseek(fd, fmap_par, SEEK_SET);
    read(fd, &mp_par, sizeof(MFS_Imap_t));
    int finode_par = mp_par.inodes[inode_num];
    if(finode_par == -1) {
        return -1;
    }


    MFS_Inode_t inode_parent;
    lseek(fd, finode_par, SEEK_SET);
    read(fd, &inode_parent, sizeof(MFS_Inode_t));

    if(inode_parent.type != MFS_DIRECTORY) {
        return -1;
    }

    char data_buf[MFS_BLOCK_SIZE];
    MFS_DirDataBlock_t* dir_buf = NULL;
    int dir_found = 0;
    int block_par = 0;
    for(int i=0; i< NUM_INODE_POINTERS; i++) {

        int fblock_par = inode_parent.data[i];
        if(fblock_par == -1) {
            continue;
        }
        block_par = i;
        lseek(fd, fblock_par, SEEK_SET);
        read(fd, data_buf, MFS_BLOCK_SIZE);
        dir_buf = (MFS_DirDataBlock_t*)data_buf;

        for(int j=0; j<NUM_DIR_ENTRIES; j++) {
            MFS_DirEnt_t* dir = &dir_buf->entries[j];
            if(dir->inum == inum) {
                dir->inum = -1;
                strcpy(dir->name, "\0");
                dir_found = 1;
                break;
            }
        }

        if(dir_found)
        break;
    }

    if(!dir_found) {
        return 0;
    }

    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, dir_buf, sizeof(MFS_DirDataBlock_t));

    MFS_Inode_t inode_parent_new;
    if ((inode_parent.size - MFS_BLOCK_SIZE) > 0 ) {
            inode_parent_new.size = inode_parent.size - MFS_BLOCK_SIZE;
    } else {
            inode_parent.size = 0;
    }

    inode_parent_new.type = MFS_DIRECTORY;
    for (int i = 0; i < 14; i++) {
        inode_parent_new.data[i] = inode_parent.data[i];
    }
    inode_parent_new.data[block_par] = p_cr->end_log;


    p_cr->end_log += MFS_BLOCK_SIZE;
    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, &inode_parent_new, sizeof(MFS_Inode_t));


    MFS_Imap_t mp_par_new;
    for(int i = 0; i< IMAP_PIECE_SIZE; i++) {
        mp_par_new.inodes[i] = mp_par.inodes[i];
    }
    mp_par_new.inodes[inode_num] = p_cr->end_log;

    p_cr->end_log += sizeof(MFS_Inode_t);
    lseek(fd, p_cr->end_log, SEEK_SET);
    write(fd, &mp_par_new, sizeof(MFS_Imap_t));

    p_cr->imap[imap_num] = p_cr->end_log;
    p_cr->end_log += sizeof(MFS_Imap_t);
    lseek(fd, 0, SEEK_SET);
    write(fd, p_cr, sizeof(MFS_CR_t));

    fsync(fd);

    return 0;
}


int server_shutdown() {
  fsync(fd);
  exit(0);
}


int create_file_image(int port, char* image_path) {
    fd = open(image_path, O_RDWR|O_CREAT, S_IRWXU);
    if (fd < 0) {
        return -1;
    }

    struct stat fs;
    if(fstat(fd,&fs) < 0) {
        return -1;
    }

    p_cr = (MFS_CR_t *)malloc(sizeof(MFS_CR_t));
    int i = 0;
	
    if(fs.st_size < sizeof(MFS_CR_t)){

        fd = open(image_path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
        if(fd <0) {
            return -1;
        }

        if(fstat(fd,&fs) < 0) {
            perror("server_init: fstat error");
            return -1;
        }
       
        p_cr->inode_count = 0;
        p_cr->end_log = sizeof(MFS_CR_t);
        for(i=0; i<NUM_IMAP; i++) {
            p_cr->imap[i] = -1;
        }

        lseek(fd, 0, SEEK_SET);
        write(fd, p_cr, sizeof(MFS_CR_t));

        MFS_DirDataBlock_t db;
        for(i=0; i< NUM_DIR_ENTRIES; i++){
            strcpy(db.entries[i].name, "\0");
            db.entries[i].inum = -1;
        }
        strcpy(db.entries[0].name, ".\0");
        db.entries[0].inum = 0; 
        strcpy(db.entries[1].name, "..\0");
        db.entries[1].inum = 0;

        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, &db, sizeof(MFS_DirDataBlock_t));

        MFS_Inode_t nd;
        nd.size = 0; 
        nd.type = MFS_DIRECTORY;
        for (i = 0; i < 14; i++) {
            nd.data[i] = -1; 
        }
        nd.data[0] = p_cr->end_log;			   

        p_cr->end_log += MFS_BLOCK_SIZE;
        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, &nd, sizeof(MFS_Inode_t));


        MFS_Imap_t mp;
        for(i = 0; i< IMAP_PIECE_SIZE; i++) {
            mp.inodes[i] = -1;
        }
        mp.inodes[0] = p_cr->end_log;

        p_cr->end_log += sizeof(MFS_Inode_t);
        lseek(fd, p_cr->end_log, SEEK_SET);
        write(fd, &mp, sizeof(MFS_Imap_t));

        
        p_cr->imap[0] = p_cr->end_log; 
        p_cr->end_log += sizeof(MFS_Imap_t);
        lseek(fd, 0, SEEK_SET);
        write(fd, p_cr, sizeof(MFS_CR_t));

        fsync(fd);
    }
    else {
        lseek(fd,0, SEEK_SET);
        read(fd, p_cr, sizeof(MFS_CR_t));
    }

    return 0;
}

int UDP_Request_Handler(int port) {
    int sd = UDP_Open(port);
    if(sd < 0){
        return -1;
    }

    struct sockaddr_in s;
    UDP_Packet rec;
    UDP_Packet send;

    while (1) {
        if( UDP_Read(sd, &s, (char *)&rec, sizeof(UDP_Packet)) < 1) {
            continue;
        }

        if(rec.request == REQ_SHUTDOWN) {
            send.request = REQ_RESPONSE;
            UDP_Write(sd, &s, (char*)&send, sizeof(UDP_Packet));
            server_shutdown();
        }
        else if(rec.request == REQ_RESPONSE) {
            send.request = REQ_RESPONSE;
            UDP_Write(sd, &s, (char*)&send, sizeof(UDP_Packet));
        }else if(rec.request == REQ_LOOKUP){
            send.inum = server_lookup(rec.inum, rec.name);
        }
        else if(rec.request == REQ_STAT){
            send.inum = server_stat(rec.inum, &(send.stat));
        }
        else if(rec.request == REQ_WRITE){
            send.inum = server_write(rec.inum, rec.buffer, rec.block);
        }
        else if(rec.request == REQ_READ){
            send.inum = server_read(rec.inum, send.buffer, rec.block);
        }
        else if(rec.request == REQ_CREAT){
            send.inum = server_creat(rec.inum, rec.type, rec.name);
        }
        else if(rec.request == REQ_UNLINK){
            send.inum = server_unlink(rec.inum, rec.name);
        }
        else {
            return -1;
        }

        send.request = REQ_RESPONSE;
        UDP_Write(sd, &s, (char*)&send, sizeof(UDP_Packet));
    }
    return 0;
}

int server_init(int port, char* image_path) {

    create_file_image(port, image_path);
    UDP_Request_Handler(port);
    
}

int main(int argc, char *argv[])
{
	if(argc != 3) {
		perror("Usage: server <portnum> <image>\n");
		exit(1);
	}

	server_init(atoi(argv[1]),argv[2] );

	return 0;
}
