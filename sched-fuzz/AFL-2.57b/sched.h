#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct ReadFrom {
  uintptr_t r;
  uintptr_t w;
} ReadFrom;

typedef struct Sched {
     uint16_t random_seed;
     uint16_t delay;
     uint16_t dist;
     bool avoid_uninit_writes;
     size_t pos_len;
     size_t neg_len;
     ReadFrom* obligations;
} Sched;

typedef uintptr_t EventId;

struct Sched* empty_sched(); 
struct Sched* mutate(Sched*); 
bool read_rf_set(char*);
void write_sched(char*, Sched*); 
void write_json_sched(const char*, Sched*); 
// Sched* read_json_sched(const char*); 
void init_prng(uint64_t); 
void use_delay(uint16_t);
void use_non_weighted_sample();
void use_l2();
void write_num_scheds(char*);
void initialize_mutator();
void always_rand();
void store_path_id(uint32_t);
uint32_t get_path_id_count(uint32_t);
uint32_t total_paths();
