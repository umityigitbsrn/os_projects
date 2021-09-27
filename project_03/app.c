

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

// #include "sbmem.h"

int main()
{
    // int i, ret;
    // char *p;
    // int ret;
    //
    // ret = sbmem_open();
    // if (ret == -1)
	  //  exit (1);
    //
    // void *all_00 = sbmem_alloc(50);
   	// void *all_01 = sbmem_alloc(50);
   	// void *all_10 = sbmem_alloc(100);
   	// void *all_11 = sbmem_alloc(100);
    // void *all_02 = sbmem_alloc(50);
    // void *all_03 = sbmem_alloc(50);
    // void *all_12 = sbmem_alloc(100);
    // void *all_13 = sbmem_alloc(100);
    // void *all_20 = sbmem_alloc(150);
    // sbmem_free(all_03);
    // sbmem_free(all_01);
    // sbmem_free(all_11);
    // sbmem_free(all_13);
    // sbmem_free(all_00);
    // sbmem_free(all_10);
    // sbmem_free(all_02);
    // sbmem_free(all_12);
    // sbmem_free(all_20);
    //
    // sbmem_close();

    pid_t x;
    x = fork();
    if (x == 0){
      execlp("./app_01", "./app_01", (char *) NULL);
    }

    x = fork();
    if (x == 0){
      execlp("./app_02", "./app_02", (char *) NULL);
    }

    x = fork();
    if (x == 0){
      execlp("./app_03", "./app_03", (char *) NULL);
    }

    wait(NULL);
    wait(NULL);
    wait(NULL);
    return (0);
}
