/*
 * Memory waster for test
 */
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <thread>
#include <vector>

static int tn = 1;
void execute_test()
{
    volatile int i;
    printf("Starting thread #%d\n", tn++);
    for (;;)
        i++;
}

int main(int ac, char *av[])
{
    char   *endp;
    size_t num_threads=0;

    if (ac < 2)
        return 1;
    num_threads = strtoull(av[1], &endp, 0);
    if (*endp)
    {
        printf("\"%s\" is threads amount\n", av[1]);
        return 1;
    }

    printf("Creating %zd threads\n", num_threads);
    std::vector<std::thread> threads;
    for (size_t i=0; i<num_threads; i++)
        threads.emplace_back(std::thread(&execute_test));

    for (size_t i = 0; i < threads.size(); i++)
        threads[i].join();

    return 0;
}
