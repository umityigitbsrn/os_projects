#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include "simplefs.h"

struct directory_item {
    char name[110];
    int inode;
};

struct file_control_block {
    int available;
    int index_block;
    int size;
};

// file index 0 + 13 blocks equals to our initial pointer location
// when we cast into our pointer to index block we get our index blocks
struct index_block { 
    int index_arr[1024];
};

// open file entry stores current file names, their modes (APPEND - 1, READ - 0)
// open file entry is basically a cache for root directory   
// inode = directory item's inode, offset points to the beginning of the directory
struct open_file_entry {
    int used;
    char name[110];
    int inode;
    int mode;
    int offset;
    int size;
    int block_count;
};

// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly. 

struct open_file_entry open_file_table[16]; // max # of currently opened files

// ========================================================

// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk. 
int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("read error\n");
	return -1;
    }
    return (0); 
}

// write block k into the virtual disk. 
int write_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("write error\n");
	return (-1);
    }
    return 0; 
}

void set_bit(void *bitmap, int nblock, unsigned int bit_index){
    void *curr = bitmap + 4096 * nblock;
    int char_index = bit_index / 8;
    int index = 7 - (bit_index % 8);
    curr = curr + char_index * sizeof(char);
    *(char *) curr |= 1UL << index;
}

int get_bit(void *bitmap, int nblock, unsigned int bit_index){
    void *curr = bitmap + 4096 * nblock;
    int char_index = bit_index / 8;
    int index = 7 - (bit_index % 8);
    curr = curr + char_index * sizeof(char);
    if ((*(char *) curr & 1UL << index) == 0)
        return 0;
    else
        return 1;
}

void clear_bit(void *bitmap, int nblock, unsigned int bit_index){
    void *curr = bitmap + 4096 * nblock;
    int char_index = bit_index / 8;
    int index = 7 - (bit_index % 8);
    curr = curr + char_index * sizeof(char);
    *(char *) curr &= ~(1UL << index);
}

void print_open_file_table(){
    int i;
    for (i = 0; i < 3; ++i)
        // if (open_file_table[i].used == 1)
            printf("index: %d name: %s, inode: %d, mode: %d, offset: %d, used: %d\n", i, 
                open_file_table[i].name, open_file_table[i].inode, open_file_table[i].mode, open_file_table[i].offset, open_file_table[i].used);
}

/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/

// this function is partially implemented.
int create_format_vdisk (char *vdiskname, unsigned int m)
{
    struct timeval current_time;
    gettimeofday(&current_time,NULL);
    double begin_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
    
    char command[1000];
    int size;
    int num = 1;
    int count;
    int stat;
    size  = num << m;
    count = size / BLOCKSIZE;
    // printf ("%d %d %d", m, size, count);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
    //printf ("executing command = %s\n", command);
    system (command);

    // now write the code to format the disk below.
    // .. your code...
    void *bitmap_buff = calloc(BLOCKSIZE, BLOCKSIZE);

    //superblock
    vdisk_fd = open(vdiskname, O_RDWR);
    void *block = calloc(BLOCKSIZE, BLOCKSIZE);
    *(int *) block = size;
    stat = write_block(block, 0);
    free(block);
    if (stat == -1){
        free(bitmap_buff);
        return -1;
    }

    //bitmaps
    unsigned int i;
    for (i = 0; i < 13; ++i)
        set_bit(bitmap_buff, 0, i);

    stat = write_block(bitmap_buff, 1);
    if (stat == -1){
        free(bitmap_buff);
        return -1;
    }

    //root directory
    block = calloc(BLOCKSIZE / 128, BLOCKSIZE);
    void *curr = block;
    for (i = 0; i < 128; ++i){
        struct directory_item dir;
        memset(dir.name, 0, sizeof(dir.name));
        dir.inode = -1;
        *(struct directory_item *) curr = dir;    
        curr += 128;
    }

    for (i = 0; i < 4; ++i){
        stat = write_block(block, (i + 5));
        if (stat == -1){
            free(block);
            free(bitmap_buff);
            return -1;
        }
    }
    free(block);
    
    // FCB
    block = calloc(BLOCKSIZE / 128, BLOCKSIZE);
    curr = block;
    
    for (i = 0; i < 128; ++i){
        struct file_control_block fcb;
        fcb.available = 0;
        *(struct file_control_block *) curr = fcb;    
        curr += 128;
    }

    for (i = 0; i < 4; ++i){
        stat = write_block(block, (i + 9));
        if (stat == -1){
            free(block);
            free(bitmap_buff);
            return -1;
        }   
    }
    free(block);

    fsync(vdisk_fd);
    close(vdisk_fd);
    free(bitmap_buff);

    gettimeofday(&current_time,NULL);
    double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
    printf("%s with size %u is created in %lf miliseconds\n", vdiskname, 1 << m, end_time - begin_time);
    return (0); 
}


// already implemented
int sfs_mount (char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it.
    
    // init open_file_table
    int i;
    for (i = 0; i < 16; ++i){
        // memset(open_file_table[i].name, 0, sizeof(open_file_table[i].name));
        open_file_table[i].inode = -1;
        open_file_table[i].used = 0;
        open_file_table[i].size = 0;
    }

    vdisk_fd = open(vdiskname, O_RDWR); 
    return(0);
}


// already implemented
int sfs_umount ()
{
    // printf("unmount\n");
    fsync (vdisk_fd); // copy everything in memory to disk
    close (vdisk_fd);
    return (0); 
}


int sfs_create(char *filename)
{
    int offset = BLOCKSIZE * 5; // points to beg. root dir
    void *buff = calloc(1, 128);
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    
    int size = 0;
    while(size < 128){
        read(vdisk_fd, buff, 128);
        if ((*(struct directory_item *) buff).inode == -1){
            strcpy((*(struct directory_item *) buff).name, filename);

            // we found the root dir
            // now we need to find our available FCB
            offset = BLOCKSIZE * 9;
            void *buff2 = calloc(1, 128);
            lseek(vdisk_fd, (off_t) offset, SEEK_SET);

            int size2 = 0;
            while (size2 < 128){
                read(vdisk_fd, buff2, 128);
                if ((*(struct file_control_block *) buff2).available == 0){
                    (*(struct directory_item *) buff).inode = size2;
                    (*(struct file_control_block *) buff2).available = 1;

                    // we found the FCB location (inode)
                    // now we need to add index_block to the FCB
                    // by checking bitmap (bit with value 0)
                    void *buff3 = calloc(1, BLOCKSIZE);
                    int i;
                    for (i = 1; i <= 4; ++i){
                        read_block(buff3, i);
                        unsigned int j;
                        for (j = 0; j < 32768; ++j){
                            if (get_bit(buff3, 0, j) == 0){
                                set_bit(buff3, 0, j);
                                write_block(buff3, i);
                                (*(struct file_control_block *) buff2).index_block = j + ((i-1) * (1 << 15));
                                (*(struct file_control_block *) buff2).size = 0;

                                void *index_buff = calloc(1, BLOCKSIZE);
                                void *index_curr = index_buff;
                                int k;
                                for (k = 0; k < (BLOCKSIZE / 4); ++k){
                                    *(int *) index_curr = -1;
                                    index_curr += sizeof(int);
                                }

                                write_block(index_buff, (*(struct file_control_block *) buff2).index_block);
                                free(index_buff);

                                offset = (9 * BLOCKSIZE) + (128 * size2);
                                lseek(vdisk_fd, (off_t) offset, SEEK_SET);
                                write(vdisk_fd, buff2, 128);

                                offset = (5 * BLOCKSIZE) + (128 * size);
                                lseek(vdisk_fd, (off_t) offset, SEEK_SET);
                                write(vdisk_fd, buff, 128);

                                free(buff3);
                                free(buff2);
                                free(buff);
                                return 0;
                            }
                        }
                    }
                    free(buff3);
                    free(buff2);
                    free(buff);
                    return -1;
                }
                size2++;
            }
            free(buff2);
            free(buff);
            return -1;
        }
        size++;
    }
    free(buff);
    return -1;
}


int sfs_open(char *file, int mode)
{
    int i;
    for (i = 0; i < 16; ++i){
        if (open_file_table[i].used == 0){
            void *block = calloc(1, BLOCKSIZE);
            void *curr;
            int j;
            for (j = 0; j < 4; ++j){
                curr = block;
                read_block(curr, j + 5);
                int k;
                for (k = 0; k < 128; ++k){
                    if (strcmp((*(struct directory_item *) curr).name, file) == 0 && (*(struct directory_item *) curr).inode != -1){
                        open_file_table[i].inode = (*(struct directory_item *) curr).inode;

                        int block_num = (*(struct directory_item *) curr).inode / 32;

                        // int fcb_offset = (block_num + 9) * BLOCKSIZE + ((*(struct directory_item *) curr).inode % 32) * 128;
                        void *fcb_buff = calloc(1, BLOCKSIZE);

                        read_block(fcb_buff, (block_num + 9));

                        // struct file_control_block* fcb_pointer = (struct file_control_block *) fcb_buff;
                        // open_file_table[i].size = (*(fcb_pointer + ((*(struct directory_item *) curr).inode % 32))).size;

                        open_file_table[i].size = (*(struct file_control_block *) (fcb_buff + ((*(struct directory_item *) curr).inode % 32) * 128)).size;
                        break;
                    }
                    curr += 128;
                }

                if (open_file_table[i].inode != -1){
                    break;    
                }
            }
            free(block);
            
            if (j >= 4) // cannot find the filename in the directory
                return -1;
                
            strcpy(open_file_table[i].name, file);
            open_file_table[i].mode = mode;
            open_file_table[i].used = 1;
            if (mode == MODE_READ)
                open_file_table[i].offset = 0;
            else
                open_file_table[i].offset = open_file_table[i].size % 4096;
            open_file_table[i].block_count = 0;
            break;
        }
    }

    // print_open_file_table();

    if (i >= 16) // all blocks are used
        return -1;
    else 
        return i; // index in the open file table
}

int sfs_close(int fd){
    open_file_table[fd].used = 0;
    
    int inode = open_file_table[fd].inode;
    int block_num = inode / 32;
    // int fcb_offset = (block_num + 9) * BLOCKSIZE + (inode % 32) * 128;
    void *fcb_buff = calloc(1, BLOCKSIZE);

    read_block(fcb_buff, (block_num + 9));

    // struct file_control_block* fcb_pointer = (struct file_control_block *) fcb_buff;
    
    // (*(fcb_pointer + (inode % 32))).size = sfs_getsize(fd);

    (*(struct file_control_block *) (fcb_buff + 128 * (inode % 32))).size = sfs_getsize(fd);

    // printf("Close getsize: %d\n", sfs_getsize(fd));

    write_block(fcb_buff, (block_num + 9));

    free(fcb_buff);
    
    return 0; 
}

int sfs_getsize (int  fd)
{
    // print_open_file_table();

    // printf("---- %d-/----\n", fd);

    int inode = open_file_table[fd].inode;
    int block_num = inode / 32;
    int fcb_offset = (block_num + 9) * BLOCKSIZE + (inode % 32) * 128;
    void *fcb_buff = calloc(1, 128);

    lseek(vdisk_fd, (off_t) fcb_offset, SEEK_SET);
    read(vdisk_fd, fcb_buff, 128);

    int index_block = (*(struct file_control_block *) fcb_buff).index_block;
    free(fcb_buff);

    void *index_buff = calloc(1, BLOCKSIZE);
    read_block(index_buff, index_block);
    void *curr = index_buff;
    
    int used = 0;

    while((used * 4) < BLOCKSIZE && *(int *) (curr) != -1){
        // printf("fd: %d, used: %d\n", fd, used);
        used++;
        curr += sizeof(int);
    }
    int offset = open_file_table[fd].offset;

    free(index_buff);
    
    if (offset != 0)
        return (used - 1) * BLOCKSIZE + offset;
    
    return used * BLOCKSIZE; 
}

int sfs_read(int fd, void *buf, int n){
    // struct timeval current_time;
    // gettimeofday(&current_time,NULL);
    // double begin_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);

    if (open_file_table[fd].mode == MODE_READ){
        int inode = open_file_table[fd].inode;
        int block_num = inode / 32;
        int fcb_offset = (block_num + 9) * BLOCKSIZE + (inode % 32) * 128;
        void *fcb_buff = calloc(1, 128);

        lseek(vdisk_fd, (off_t) fcb_offset, SEEK_SET);
        read(vdisk_fd, fcb_buff, 128);

        int index_block = (*(struct file_control_block *) fcb_buff).index_block;
        free(fcb_buff);

        // int size = 0;
        void *read_buff_index = calloc(1, BLOCKSIZE);
        read_block(read_buff_index, index_block);

        int i, initial = 0;        
        for(i = 0; i < 1024 && initial < (open_file_table[fd].block_count); i++){
            if (*(((int *)read_buff_index) + i) != -1){
                initial++;
            } else {
                return -1;
            }
        }
        int remain_size = open_file_table[fd].size - (open_file_table[fd].block_count * BLOCKSIZE + open_file_table[fd].offset);
        int remain_read = n;

        int initial_size = remain_size;

        if (remain_size == 0){
            // gettimeofday(&current_time,NULL);
            // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
            // printf("Reading %d size from file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);   
            return 0;
        }

        // printf("Initial offset value: %d\n", open_file_table[fd].offset);

        void *read_buff = calloc(1, BLOCKSIZE);
        for(i = initial; i < 1024; i++){
            if(*(((int *)read_buff_index) + i) != -1){
                read_block(read_buff, i);
                // printf("remain read: %d, remain_size: %d\n", remain_read, remain_size);
                if (remain_read < remain_size){
                    if (4096 - open_file_table[fd].offset <= remain_read){
                        // printf("IF - 1\n");
                        memcpy(buf + (n - remain_read), read_buff + open_file_table[fd].offset, 4096 - open_file_table[fd].offset);
                        remain_size -= 4096 - open_file_table[fd].offset; 
                        remain_read -= 4096 - open_file_table[fd].offset;
                        open_file_table[fd].offset = 0;
                        open_file_table[fd].block_count++;
                    } else {
                        // printf("IF - 2\n");
                        memcpy(buf + (n - remain_read), read_buff + open_file_table[fd].offset, remain_read);
                        open_file_table[fd].offset = open_file_table[fd].offset + remain_read;
                        free(read_buff);
                        // gettimeofday(&current_time,NULL);
                        // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
                        // printf("Reading %d size from file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
                        return n;
                    }
                } else {
                    if (remain_size > 4096 - open_file_table[fd].offset){
                        // printf("IF - 3\n");
                        memcpy(buf + (n - remain_read), read_buff + open_file_table[fd].offset, 4096 - open_file_table[fd].offset);
                        remain_size -= 4096 - open_file_table[fd].offset; 
                        remain_read -= 4096 - open_file_table[fd].offset;
                        open_file_table[fd].offset = 0;
                        open_file_table[fd].block_count++;
                    } else {
                        // printf("IF - 4\n");
                        memcpy(buf + (n - remain_read), read_buff + open_file_table[fd].offset, remain_size);
                        open_file_table[fd].offset += remain_size;
                        free(read_buff);
                        // gettimeofday(&current_time,NULL);
                        // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
                        // printf("Reading %d size from file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
                        return initial_size;
                    }
                }
            } else {
                printf("Unexpected condition.\n");
                free(read_buff);
                return -1;
            }
        }
    }

    return -1;
}


int sfs_append(int fd, void *buf, int n)
{
    // struct timeval current_time;
    // gettimeofday(&current_time,NULL);
    // double begin_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);

    if (open_file_table[fd].mode == MODE_APPEND){
        int inode = open_file_table[fd].inode;
        int block_num = inode / 32;
        int fcb_offset = (block_num + 9) * BLOCKSIZE + (inode % 32) * 128;
        void *fcb_buff = calloc(1, 128);

        lseek(vdisk_fd, (off_t) fcb_offset, SEEK_SET);
        read(vdisk_fd, fcb_buff, 128);

        int index_block = (*(struct file_control_block *) fcb_buff).index_block;
        free(fcb_buff);

        int size = n;
        void *buff = calloc(1, BLOCKSIZE);
        read_block(buff, index_block);
        int i;
        for (i = 0; size > 0 && i < 1023; ++i){ // 1024 case
            if ((open_file_table[fd].offset == 0 && *(int *) (buff + i * sizeof(int)) == -1) ||
                (open_file_table[fd].offset > 0 && *(int *) (buff + i * sizeof(int)) != -1 && *(int *) (buff + (i + 1) * sizeof(int)) == -1)){
                    if (*(int *) (buff + i * sizeof(int)) == -1){
                        void *bitmap_buff = calloc(1, BLOCKSIZE);
                        // Now we need to find empty block 

                        int b, x = 1, t = 0;
                        while(x <= 4 && !t){
                            read_block(bitmap_buff, x);
                        
                            for(b = 0; b < 32768; b++){
                                if(get_bit(bitmap_buff, 0, b) == 0){
                                    set_bit(bitmap_buff,0, b);
                                    write_block(bitmap_buff, x);

                                    int* as = (int *)buff;
                                    as[i] = b;
                                    t = 1;
                                    write_block(buff, index_block);
                                    break;
                                }
                            }
                            x++;
                            free(bitmap_buff);
                        }

                        if(!t){
                            printf("NO SPACE AVAILABLE\n");
                            
                            free(buff);
                            return -1;
                        }
                    }
                

                    void *file_writer = calloc(1, BLOCKSIZE);
                    
                    int *block_loc = (int *)buff;

                    read_block(file_writer, block_loc[i]);
                    
                    if(size <= 4096 - open_file_table[fd].offset){
                        //fits the current block

                        memcpy(file_writer + open_file_table[fd].offset, buf + n - size, size);

                        open_file_table[fd].offset = (open_file_table[fd].offset + size) % 4096;

                        write_block(file_writer, block_loc[i]);

                        size = 0;

                        free(file_writer);
                        free(buff);
                        // gettimeofday(&current_time,NULL);
                        // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
                        // printf("Appending %d size to file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
                        return n;
                    }
                    else{
                        memcpy(file_writer + open_file_table[fd].offset, buf + n - size, 4096 - open_file_table[fd].offset);

                        size = size - 4096 + open_file_table[fd].offset;

                        open_file_table[fd].offset = 0; 

                        write_block(file_writer, block_loc[i]);

                        free(file_writer);
                    }

            }
        }

        if(*(int *) (buff + 1023 * sizeof(int)) == -1){
            void *bitmap_buff = calloc(1, BLOCKSIZE);
            // Now we need to find empty block 

            int b, x = 1, t = 0;
            while(x <= 4 && !t){
                read_block(bitmap_buff, x);
            
                for(b = 0; b < 32768; b++){
                    if(get_bit(bitmap_buff, 0, b) == 0){
                        set_bit(bitmap_buff,0, b);
                        write_block(bitmap_buff, x);

                        int* as = (int *)buff;
                        as[i] = b;
                        t = 1;
                        write_block(buff, index_block);
                        break;
                    }
                }
                x++;
                free(bitmap_buff);
            }
            if(!t){
                printf("NO SPACE AVAILABLE\n");
                
                free(buff);
                return -1;
            }
            void *file_writer = calloc(1, BLOCKSIZE);

            read_block(file_writer, *(int *) (buff + 1023 * sizeof(int)));

            if(size > 4096){
                memcpy(file_writer, buf + n - size, 4096);

                size -= 4096;
                
                write_block(file_writer, *(int *) (buff + 1023 * sizeof(int)));

                free(file_writer);
                free(buff);
                // gettimeofday(&current_time,NULL);
                // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
                // printf("Appending %d size to file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
                return n-size;
            }
            
            memcpy(file_writer, buf + n - size, size);

            open_file_table[fd].offset = size;

            write_block(file_writer, *(int *) (buff + 1023 * sizeof(int)));

            free(file_writer);
            free(buff);
            // gettimeofday(&current_time,NULL);
            // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
            // printf("Appending %d size to file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
            return n;
        }
       
        else if((open_file_table[fd].offset > 0 && *(int *) (buff + 1023 * sizeof(int)) != -1)){
            void *file_writer = calloc(1, BLOCKSIZE);

            read_block(file_writer, *(int *) (buff + 1023 * sizeof(int)));
            
            if(size > 4096 - open_file_table[fd].offset){
                memcpy(file_writer + open_file_table[fd].offset, buf + n - size, 4096 - open_file_table[fd].offset);

                open_file_table[fd].offset = 0;
                size = size - 4096 + open_file_table[fd].offset;
                write_block(file_writer, *(int *) (buff + 1023 * sizeof(int)));

                free(file_writer);
                free(buff);
                // gettimeofday(&current_time,NULL);
                // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
                // printf("Appending %d size to file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
                return n - size;
            }else{
                memcpy(file_writer + open_file_table[fd].offset, buf + n - size, size);

                write_block(file_writer, *(int *) (buff + 1023 * sizeof(int)));

                free(file_writer);
                free(buff);
                // gettimeofday(&current_time,NULL);
                // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
                // printf("Appending %d size to file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
                return n;
            }


        }

        free(buff);
        // gettimeofday(&current_time,NULL);
        // double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
        // printf("Appending %d size to file with fd: %d in %lf miliseconds\n", n, fd, end_time - begin_time);
        return n - size;
        // check the return value
    } else
        return -1; 
}

int sfs_delete(char *filename)
{
    void *buff = calloc(1, BLOCKSIZE);
    int i;
    for (i = 0; i < 4; ++i){
        read_block(buff, i + 5);
        int j;
        for (j = 0; j < 32; ++j){
            if (strcmp((*(struct directory_item *) (buff + j * 128)).name, filename) == 0){
                int inode = (*(struct directory_item *) (buff + j * 128)).inode;
                (*(struct directory_item *) (buff + j * 128)).inode = -1;
                write_block(buff, i + 5);

                int block_num = inode / 32;
                int block_offset = inode % 32;
                read_block(buff, block_num + 9);
                int index_block = (*(struct file_control_block *) (buff + 128 * block_offset)).index_block;
                (*(struct file_control_block *) (buff + 128 * block_offset)).available = 0;
                // (*(struct file_control_block *) (buff + 128 * block_offset)).size = 0;

                write_block(buff, block_num + 9);

                read_block(buff, index_block);
                int *curr = (int *) buff;
                int k;
                for (k = 0; k < 1024 && curr[k] != -1; ++k){
                    int bitmap_index = curr[k];
                    int bitmap_block = bitmap_index / (1 << 15);
                    int bitmap_offset = bitmap_index % (1 << 15);
                    void *bitmap_buff = calloc(1, BLOCKSIZE);
                    read_block(bitmap_buff, bitmap_block + 1);
                    clear_bit(bitmap_buff, 0, bitmap_offset);
                    write_block(bitmap_buff, bitmap_block + 1);
                    free(bitmap_buff);
                    curr[k] = -1;
                }

                int bitmap_index = index_block;
                int bitmap_block = bitmap_index / (1 << 15);
                int bitmap_offset = bitmap_index % (1 << 15);
                void *bitmap_buff = calloc(1, BLOCKSIZE);
                read_block(bitmap_buff, bitmap_block + 1);
                clear_bit(bitmap_buff, 0, bitmap_offset);
                write_block(bitmap_buff, bitmap_block + 1);
                free(bitmap_buff);

                write_block(buff, index_block);

                free(buff);
                return 0;
            }
        }
    }

    free(buff);

    return (-1); 
}

