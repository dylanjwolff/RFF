
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

static size_t sum = 0;
static size_t dif = 0;
static size_t input = 0;

static void *sub_worker(void *arg) {
    for (uint32_t i = 0; i < input; i++) {
        dif -= 1;
        sum -= 1;
    }
}

static void *add_worker(void *arg)
{
    for (uint32_t i = 0; i < input; i++) {
        sum += 1;
        dif += 1;
    }
}

int main(int argc, char **argv)
{
    input = atoi(getenv("INPUT"));

    pthread_t adder;
    pthread_t suber;
    pthread_create(&adder, NULL, add_worker, NULL);
    pthread_create(&suber, NULL, sub_worker, NULL);
    pthread_join(adder, NULL);
    pthread_join(suber, NULL);

    if (dif > 4) {
          fprintf(stderr, "bug found!");
          abort();
    }

    printf("sum = %zu , dif = %zu \n", sum, dif); 
    return 0;
}

