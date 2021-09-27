#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "sbmem.h"

int main()
{
    // int i, ret;
    // char *p;
    int ret;

    ret = sbmem_open();
    if (ret == -1)
	   exit (1);

    void *all_00 = sbmem_alloc(50);
   	void *all_01 = sbmem_alloc(50);
   	void *all_10 = sbmem_alloc(100);
   	// void *all_11 = sbmem_alloc(100);
    // void *all_02 = sbmem_alloc(50);
    // void *all_03 = sbmem_alloc(50);
    // void *all_12 = sbmem_alloc(100);
    // void *all_13 = sbmem_alloc(100);
    // void *all_20 = sbmem_alloc(150);
    // sbmem_free(all_03);
    sbmem_free(all_01);
    // sbmem_free(all_11);
    // sbmem_free(all_13);
    sbmem_free(all_00);
    sbmem_free(all_10);
    // sbmem_free(all_02);
    // sbmem_free(all_12);
    // sbmem_free(all_20);

    sbmem_close();

    return (0);
}
