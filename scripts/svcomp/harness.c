#include<stdint.h>
#include<stdio.h>
#include<assert.h>
#include<stdbool.h>
#include<pthread.h>

const uint32_t  buf_size_bytes = 1000;
const uint8_t* buf[buf_size_bytes];
size_t num_input_bytes;
uint8_t* current = 0;
bool is_init = false;
pthread_mutex_t harness_mutex;

int count = 0;

void harness_init() {
    assert(is_init == false);
    FILE* fp = fopen("input.bin","rb");
    num_input_bytes = fread(buf, 1, 1000, fp);
    assert(num_input_bytes > 0);
    current = (uint8_t*) buf;
    assert(current > 0);
    is_init = true;
    // fprintf(stderr, "inited \n");

}

unsigned int __VERIFIER_nondet_uint() {
        assert(is_init == true);

        // fprintf(stderr, "acquiring \n");
        pthread_mutex_lock(&harness_mutex);
        count = count + 1;
        // fprintf(stderr, "%d count\n", count);

        if (current + sizeof(unsigned int) >= buf + num_input_bytes) {
            current = (uint8_t*) buf;
        }

        unsigned int* ptr = (unsigned int*) current;
        unsigned int v = *ptr;
        current = current + sizeof(unsigned int);

        // fprintf(stderr, "%d ret\n", v);
        pthread_mutex_unlock(&harness_mutex);

        return v;
}

int __VERIFIER_nondet_int() {
        return (int) __VERIFIER_nondet_uint();
}

unsigned char __VERIFIER_nondet_char() {
        assert(is_init == true);

        pthread_mutex_lock(&harness_mutex);
        count = count + 1;
        // fprintf(stderr, "%d count\n", count);

        if (current + sizeof(unsigned int) >= buf + num_input_bytes) {
            current = (uint8_t*) buf;
        }

        char* ptr = (char *) current;
        char v = *ptr;
        current = current + sizeof(char);

        // fprintf(stderr, "0x%02X ret\n", v);
        pthread_mutex_unlock(&harness_mutex);

        return v;
}

bool __VERIFIER_nondet_bool() {
        assert(is_init == true);

        pthread_mutex_lock(&harness_mutex);
        count = count + 1;
        // fprintf(stderr, "%d count\n", count);

        if (current + sizeof(unsigned int) >= buf + num_input_bytes) {
            current = (uint8_t*) buf;
        }

        bool* ptr = (bool*) current;
        bool v = *ptr;
        current = current + sizeof(bool);

        fprintf(stderr, "0x%02X ret\n", v);
        pthread_mutex_unlock(&harness_mutex);

        return v;
}

void __VERIFIER_atomic_begin() {
        // fprintf(stderr, "\n\nreach begins\n\n");
        pthread_mutex_lock(&harness_mutex);
        count = count + 1;
        setenv("ATOMIC", "1");
        char* e = getenv("ATOMIC");
        if (e) {
            // fprintf(stderr, "env was %s\n", e);
        }
        pthread_mutex_unlock(&harness_mutex);
}

void __VERIFIER_atomic_end() {
        // fprintf(stderr, "\n\nreach ends\n\n");
        pthread_mutex_lock(&harness_mutex);
        char* e = getenv("ATOMIC");
        if (e) {
            // fprintf(stderr, "env was %s\n", e);
        }
        setenv("ATOMIC", "0");
        pthread_mutex_unlock(&harness_mutex);
}
