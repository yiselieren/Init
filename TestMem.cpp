/*
 * Memory waster for test
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define M 1048576
int main(int ac, char *av[])
{
    char   *endp, *mem_region;
    size_t mem_size;

    if (ac < 2)
        return 1;
    mem_size = strtoull(av[1], &endp, 0);
    if (*endp)
    {
        printf("\"%s\" is invalid allocation size\n", av[1]);
        return 1;
    }

    printf("Allocating %zd megabytes of memory\n", mem_size);
    mem_size *= M;
    mem_region = (char*)malloc(mem_size);
    if (!mem_region)
    {
        printf("Can't allocate %zd bytes of memory\n", mem_size);
        return 1;
    }

    for (;;)
    {
        size_t i;

        for (i=0; i<mem_size; i++)
            mem_region[i] = rand() & 0xff;

        sleep(1);
    }

    return 0;
}
