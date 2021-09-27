#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include "sbmem.h"

int main()
{

    sbmem_init(32768);

    printf ("memory segment is created and initialized \n");

    return (0);
}
