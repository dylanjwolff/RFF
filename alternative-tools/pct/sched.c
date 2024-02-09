
#define LIBDL 1

#include "stdlib.c"
#include "AFL-2.57b/sched.h"

#include <pthread.h>
#include <sys/syscall.h>
#include <assert.h>

#define gettid() syscall(SYS_gettid)

#define RED "\33[31m"
#define OFF "\33[0m"

#define AREA ((uint8_t *)0x1B0000)

#define AFL_BITMAP_SIZE (1 << 16)
#define SCHED_BITMAP_SIZE (1 << 17)

static uint8_t *sched_bitmap = NULL;

typedef size_t (*switch_t)(const void *, const void *, size_t, bool, const void *);
typedef struct Sched (*get_sched_t)();
typedef void (*rem_t)(EventId, EventId);

static struct Sched *sched;
static size_t counter = 0;
static switch_t switch_func = NULL;
static get_sched_t get_sched_func = NULL;
static rem_t rem_func = NULL;

FILE *logfile = NULL;
char* log_dir = NULL;
bool should_log = false;
bool placeholder_log_indicator_shm = false;
bool* log_indicator = false;

static bool POS_ONLY = 0;
static bool NO_RFF = 0;

#define E_CK = 1

static inline bool ck_pos_only() {
    #ifdef E_CK
    return POS_ONLY;
    #else
    return false;
    #endif
}

static inline bool ck_rff() {
    #ifdef E_CK
    return NO_RFF;
    #else
    return false;
    #endif
}

static unsigned int rand_state = 0;

static inline void srand(unsigned int seed)
{
    rand_state = seed;
}

// GLIBC
static inline unsigned int rand()
{
    rand_state = 1103515245 * rand_state + 12345;
    return rand_state;
}

/*
 * Set a thread switch coverage bit.
 */
static void set(uint16_t idx)
{
    idx >>= 1;
    AREA[idx / 8] |= (1 << (idx % 8));
}

inline size_t hash_combine(uintptr_t lhs,  uintptr_t rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

static const char R = 'r';
static const char W = 'w';
static const char RW = 'z';

static inline void log_op(char op_class, const void *instr_addr, const char *asm_op, const void *mem_addr, pid_t tid)
{
    if (should_log){
    logfile = fopen(log_dir, "a+");
    if(logfile != NULL) {
            if(asm_op[0] == 'l' && asm_op[1] == 'o' && asm_op[2] == 'c' && asm_op[3] == 'k')
            {
                char* opCode;
                if(op_class == 'r') opCode = "a_r";
                else if(op_class == 'w') opCode = "a_w";
                else opCode = "a_rw";
                fprintf(logfile, "[%lu] %s: %lu|%s(%lu)|%d\n",
                                instr_addr, asm_op, tid, opCode, mem_addr, 0);
            }
            else
            {
                if (op_class != W){
                        fprintf(logfile, "[%lu] %s: %lu|r(%lu)|%d\n",
                                instr_addr, asm_op, tid, mem_addr, 0);
                }

                if (op_class != R){
                        fprintf(logfile, "[%lu] %s: %lu|w(%lu)|%d\n",
                                instr_addr, asm_op, tid, mem_addr, 0);
                }            }
            fclose(logfile);
    }
    }
}

static inline void dont_double_switch() {
    if (counter == 0) {
        counter = 1;
    }
}

static inline void switch_if_counter(const void *instr_addr, const void *mem_addr, size_t size, bool is_write, const void *func)
{
    dlcall(switch_func, instr_addr, mem_addr, size, is_write, func, NULL, false);
    return;
    if (counter == 0)
    {
        counter = rand() % sched->delay;
        dlcall(switch_func, instr_addr, mem_addr, size, is_write, func, NULL, false);
    }
    else
    {
        counter--;
    }
}

void mem_wri(const char *asm_op, const void *instr_addr, const void *mem_addr, size_t size, const void *func)
{
    // check_obligations(instr_addr, mem_addr, size, func);
    // check_negations(instr_addr, mem_addr, size, func);

    // avoid_writes(instr_addr, mem_addr, size, func);
    // switch_if_avoid_all_writes(instr_addr, mem_addr, size, func);

    switch_if_counter(instr_addr, mem_addr, size, true, func);
    // --- If we are going to switch, we have done so by this point ---

    pid_t tid = gettid();
    // update_read_data(instr_addr, mem_addr, tid); // assume read occurs before write
    // update_write_data(instr_addr, mem_addr, tid);
    log_op(RW, instr_addr, asm_op, mem_addr, tid);
}

void mem_ri(const char *asm_op, const void *instr_addr, const void *mem_addr, size_t size, const void *func)
{
    // check_obligations(instr_addr, mem_addr, size, func);
    // check_negations(instr_addr, mem_addr, size, func);

    switch_if_counter(instr_addr, mem_addr, size, false, func);
    // --- If we are going to switch, we have done so by this point ---

    pid_t tid = gettid();
    // update_read_data(instr_addr, mem_addr, tid);
    log_op(R, instr_addr, asm_op, mem_addr, tid);
}

void mem_wi(const char *asm_op, const void *instr_addr, const void *mem_addr, size_t size, const void *func)
{
    // avoid_writes(instr_addr, mem_addr, size, func);
    // switch_if_avoid_all_writes(instr_addr, mem_addr, size, func);

    switch_if_counter(instr_addr, mem_addr, size, true, func);
    // --- If we are going to switch, we have done so by this point ---
    pid_t tid = gettid();
    // update_write_data(instr_addr, mem_addr, tid);
    log_op(W, instr_addr, asm_op, mem_addr, tid);
}

/*
 * Init.
 */
void init(int argc, const char **argv, char **envp, void *dynp)
{
    environ = envp;

    const char *filename = getenv("LD_PRELOAD");
    if (filename == NULL)
    {
        fprintf(stderr, RED "error" OFF ": LD_PRELOAD should be set to "
                            "\"$PWD/libsched.so\"\n");
        abort();
    }
    dlinit(dynp);
    void *handle = dlopen(filename, RTLD_NOW);
    if (handle == NULL)
    {
        fprintf(stderr, RED "error" OFF ": failed to open file \"%s\"\n",
                filename);
        abort();
    }

    const char *funcname = "thread_switch";
    switch_func = dlsym(handle, funcname);
    if (switch_func == NULL)
    {
        fprintf(stderr, RED "error" OFF ": failed to find function \"%s\"\n",
                funcname);
        abort();
    }
    const char *get_sched_funcname = "get_sched";
    get_sched_func = dlsym(handle, get_sched_funcname);
    if (get_sched_func == NULL)
    {
        fprintf(stderr, RED "error" OFF ": failed to find function \"%s\"\n",
                get_sched_funcname);
        abort();
    }
    const char *rem_funcname = "remove_pending_obligation";
    rem_func = dlsym(handle, rem_funcname);
    if (rem_func == NULL)
    {
        fprintf(stderr, RED "error" OFF ": failed to find function \"%s\"\n",
                rem_funcname);
        abort();
    }

    dlclose(handle);

    const char *shm_id_env = getenv("__AFL_SHM_ID");
    if (shm_id_env == NULL)
    {
        sched_bitmap = (uint8_t *)malloc(SCHED_BITMAP_SIZE);
        log_indicator = &placeholder_log_indicator_shm;
    }
    else
    {
        int shm_id = atoi(shm_id_env);
        sched_bitmap = ((uint8_t *)shmat(shm_id, NULL, 0)) + AFL_BITMAP_SIZE;
        log_indicator = sched_bitmap + SCHED_BITMAP_SIZE + 1;
    }

    char* pos_only = getenv("POS_ONLY");
    POS_ONLY = (pos_only && strcmp(pos_only, "0"));

    char* no_rff = getenv("NO_RFF");
    NO_RFF = (no_rff && strcmp(no_rff, "0"));

    sched = (Sched*) dlcall(get_sched_func);
    fprintf(stderr, "sched rand %hu delay %hu len %hu %hu\n", sched->random_seed, sched->delay, sched->pos_len, sched->neg_len);
    srand((unsigned int)sched->random_seed);

    char *to_log = getenv("TO_LOG");
    log_dir = getenv("LOG_DIR");
    if (*log_indicator) {
        to_log = "true";
        log_dir = "events.log";
    }

    if (to_log)
    {
        if (log_dir == NULL)
        {
            abort();
        };
        should_log = true;
    }
}
