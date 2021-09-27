// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <string.h>
// #include "simplefs.h"

// int main(int argc, char **argv)
// {
//     int ret;
//     int fd1, fd2, fd; 
//     int i;
//     char c; 
//     char buffer[1024];
//     char buffer2[8] = {50, 50, 50, 50, 50, 50, 50, 50};
//     int size;
//     char vdiskname[200]; 

//     printf ("started\n");

//     if (argc != 2) {
//         printf ("usage: app  <vdiskname>\n");
//         exit(0);
//     }
//     strcpy (vdiskname, argv[1]); 
    
//     ret = sfs_mount (vdiskname);
//     if (ret != 0) {
//         printf ("could not mount \n");
//         exit (1);
//     }

//     printf ("creating files\n"); 
//     printf("%d\n", sfs_create ("file1.bin"));
//     printf("%d\n", sfs_create ("file2.bin"));
//     // printf("%d\n", sfs_create ("file3.bin"));

//     fd1 = sfs_open ("file1.bin", MODE_APPEND);
//     fd2 = sfs_open ("file2.bin", MODE_APPEND);
//     // fd = sfs_open("file3.bin", MODE_READ);

//     // printf("File descriptor - 1: %d\n", fd1);
//     // printf("File descriptor - 2: %d\n", fd2);
//     // printf("File descriptor - 3: %d\n", fd);

//     printf("Size of file - 2- before: %d\n", sfs_getsize(fd2));

//     for (i = 0; i < 5000; ++i) {
//         buffer[0] = (char) 65;
//         buffer[1] = (char) 66;
//         buffer[2] = (char) 67;
//         buffer[3] = (char) 68;
//         sfs_append(fd2, (void *) buffer, 4);
//     }

//     for (i = 0; i < 10000; ++i) {
//         buffer[0] = (char) 69;
//         buffer[1] = (char) 70;
//         buffer[2] = (char) 71;
//         sfs_append (fd1, (void *) buffer, 3);
//     }

//     for (i = 0; i < 5000; ++i) {
//         buffer[0] = (char) 65;
//         buffer[1] = (char) 66;
//         buffer[2] = (char) 67;
//         buffer[3] = (char) 68;
//         sfs_append(fd2, (void *) buffer, 4);
//     }

//     // printf("File descriptor - 1 - After: %d\n", fd1);
//     // printf("File descriptor - 2 - After: %d\n", fd2);
    
//     printf("-------------------------\n");

//     printf("Size of file - 1: %d\n", sfs_getsize(fd1));
//     printf("Size of file - 2: %d\n", sfs_getsize(fd2));
    
//     // printf("-------------------------\n");
    
//     sfs_close(fd1);

//     sfs_close(fd2);
    
//     // printf("-------------------------\n");
    
//     fd1 = sfs_open("file1.bin", MODE_READ);
    
//     // print_open_file_table();

//     fd2 = sfs_open("file2.bin", MODE_APPEND);

//     // print_open_file_table();

//     printf("-------------------------\n");

//     // printf("File descriptor - 1 - After open: %d\n", fd1);
//     // printf("File descriptor - 2 - After open: %d\n", fd2);
    
//     // printf("-------------------------\n");
    
//     sfs_read(fd1, (void *) buffer, 1);
//     sfs_read(fd2, (void *) buffer, 1);
//     // sfs_delete("file1.bin");
//     // sfs_delete("file2.bin");

//     // fd = sfs_open("file3.bin", MODE_APPEND);
//     // for (i = 0; i < 10000; ++i) {
//     //     memcpy (buffer, buffer2, 8); // just to show memcpy
//     //     sfs_append(fd, (void *) buffer, 8);
//     // }
//     // sfs_close (fd);

//     // fd = sfs_open("file3.bin", MODE_READ);
//     // size = sfs_getsize (fd);
//     // for (i = 0; i < size; ++i) {
//     //     sfs_read (fd, (void *) buffer, 1);
//     //     c = (char) buffer[0];
//     //     c = c + 1;
//     // }
//     // sfs_close (fd);
//     // ret = sfs_umount();
// }

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "simplefs.h"

int main(int argc, char **argv)
{
    // int ret;
    // int fd1, fd2, fd; 
    // int i;
    // // char c; 
    // char buffer[1024];
    // char buffer_n[10000];
    // char buffer_n2[15000];
    // char buffer2[8] = {50, 50, 50, 50, 50, 50, 50, 50};
    // // int size;

    int fd, ret, i, append_count = 0, read_count = 0;
    char vdiskname[200];
    char write_buff[1024];
    char read_buff[1024]; 

    printf ("started\n");

    if (argc != 2) {
        printf ("usage: app  <vdiskname>\n");
        exit(0);
    }
    strcpy (vdiskname, argv[1]); 
    
    for (i = 0; i < 64; ++i){
        write_buff[i] = (char) i;
    }

    ret = sfs_mount (vdiskname);
    if (ret != 0) {
        printf ("could not mount \n");
        exit (1);
    }

    sfs_create("file.bin");

    fd = sfs_open("file.bin", MODE_APPEND);

    struct timeval current_time;
    gettimeofday(&current_time,NULL);
    double begin_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);

    for (i = 0; i < 64; ++i){
        append_count += sfs_append(fd, write_buff, 1024);
    }

    gettimeofday(&current_time,NULL);
    double end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
    printf("Appending %d size to file with fd: %d in %lf miliseconds\n", append_count, fd, end_time - begin_time);

    sfs_close(fd);

    sfs_open("file.bin", MODE_READ);

    gettimeofday(&current_time,NULL);
    begin_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);

    for (i = 0; i < 64; ++i){
        read_count += sfs_read(fd, read_buff, 1024);
    }

    gettimeofday(&current_time,NULL);
    end_time = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
    printf("Reading %d size from file with fd: %d in %lf miliseconds\n", append_count, fd, end_time - begin_time);

    // sfs_close(fd);
    
    // sfs_delete("file.bin");
    sfs_umount();

    // printf ("creating files\n"); 
    // sfs_create ("file1.bin");
    // sfs_create ("file2.bin");
    // sfs_create ("file3.bin");

    // fd1 = sfs_open ("file1.bin", MODE_APPEND);
    // fd2 = sfs_open ("file2.bin", MODE_APPEND);
    // for (i = 0; i < 10000; ++i) {
    //     buffer[0] =   (char) 65;
    //     sfs_append (fd1, (void *) buffer, 1);
    // }

    // sfs_close(fd1);

    // fd1 = sfs_open("file1.bin", MODE_READ);

    // printf("Read size for file1.bin: %d byte.\n", sfs_read(fd1, buffer_n2, 15000));

    // for (i = 0; i < 10000; ++i) {
    //     buffer[0] = (char) 65;
    //     buffer[1] = (char) 66;
    //     buffer[2] = (char) 67;
    //     buffer[3] = (char) 68;
    //     sfs_append(fd2, (void *) buffer, 4);
    // }
    
    // sfs_close(fd1);
    // sfs_close(fd2);

    // fd = sfs_open("file3.bin", MODE_APPEND);
    // for (i = 0; i < 10000; ++i) {
    //     memcpy (buffer, buffer2, 8); // just to show memcpy
    //     sfs_append(fd, (void *) buffer, 8);
    // }
    // sfs_close (fd);

    // fd = sfs_open("file3.bin", MODE_READ);
    // // size = sfs_getsize (fd);
    // // for (i = 0; i < size; ++i) {
    // //     sfs_read (fd, (void *) buffer, 1);
    // //     c = (char) buffer[0];
    // //     c = c + 1;
    // // }

    // for (int umit = 0; umit < 12; umit++){
    //     printf("Read output: %d\n", sfs_read(fd, buffer_n, 8000));
    // }

    // printf("exit\n");

    // sfs_close (fd);

    // sfs_delete("file1.bin");
    // sfs_delete("file2.bin");
    // sfs_delete("file3.bin");

    // printf("close\n");
    // ret = sfs_umount();
}