
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

static size_t sum = 0;
static uint32_t input = 0;

static void *sub_worker(void *arg) {
    if (input < 0) {
        sum += 1;
        sum += 2;
        sum += 3;
        sum += 4;
        sum += 5;
        sum += 6;
        sum += 7;
        sum += 8;
        sum += 9;
    }
}

static void *add_worker(void *arg)
{
    if (input < 0) {
        sum += 1;
        sum += 2;
        sum += 3;
        sum += 4;
        sum += 5;
        sum += 6;
        sum += 7;
        sum += 8;
        sum += 9;
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

    return 0;
}

