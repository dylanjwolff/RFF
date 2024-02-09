
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

/*
 * Sums input read from a file and prints the result.
 * Uses pthreads so it is super fast.
 */

static size_t sum = 0;
#ifdef SAFE
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void *worker(void *arg)
{
    while (true)
    {
        char c = getc(stdin);
        if (c == EOF)
            break;

#ifdef SAFE
        pthread_mutex_lock(&mutex);
#endif

        /*
         * XXX: Temporary hack to separate reads from writes.
         */
        asm volatile (
            "mov %0, %%r11\n"
            "add %%r11, %1\n"
            "mov %1, %0" : : "m"(sum), "r"((size_t)(uint8_t)c): "r11");
        // sum += (size_t)(uint8_t)c;

#ifdef SAFE
        pthread_mutex_unlock(&mutex);
#endif

    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "usage: %s file [num-threads]\n", argv[0]);
        return 1;
    }

    int n = 0;
    if (argc == 3)
        n = atoi(argv[2]);
    if (n <= 0)
        n = 4;

    FILE *stream = freopen(argv[1], "r", stdin);
    if (stream == NULL)
    {
        fprintf(stderr, "error: failed to open file \"%s\": %s",
            argv[1], strerror(errno));
        abort();
    }

    pthread_t threads[n];
    for (int i = 0; i < n; i++)
    {
        int r = pthread_create(threads + i, NULL, worker, NULL);
        if (r < 0)
        {
            fprintf(stderr, "error: failed to create thread\n");
            abort();
        }
    }
    for (int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);

    printf("sum = %zu\n", sum);
    return 0;
}

