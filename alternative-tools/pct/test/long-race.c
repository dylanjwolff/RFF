
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

static size_t sum = 0;
static size_t dif = 0;

static void *sub_worker(void *arg) {
    if (sum == 1) { // 4199176
        dif -= 1; // 4199191 4199205
        if (sum == 2) { // 4199213
            dif -= 1; // 4199228 4199242
            if (sum == 3) { // 4199250
                dif -= 1; // 4199265 4199279
                if (sum == 4) { // 4199287
                    dif -= 1; // 4199302 4199316
                }
            }
        }
    }

    return NULL;
}

static void *add_worker(void *arg)
{
    sum += 1; // 4198956 4198970

    if (dif == -1) { // 4198978
        sum += 1; // 4198993 4199007
        if (dif == -2) { // 4199015
            sum += 1; // 4199030 4199044
            if (dif == -3) { // 4199052
                sum += 1; // 4199067 4199081
                if(dif == -4) { // 4199089
                    fprintf(stderr, "Bug found\n");
                    abort();
                }
            }
        }
    }
        
    return NULL;
}

int main(int argc, char **argv)
{
    int n = 2;

    pthread_t adder;
    pthread_t suber;
    pthread_create(&adder, NULL, add_worker, NULL);
    pthread_create(&suber, NULL, sub_worker, NULL);
    pthread_join(adder, NULL);
    pthread_join(suber, NULL);

    printf("sum = %zu\n", sum); // 4198890
    return 0;
}

