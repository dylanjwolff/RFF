
#define LIBDL 1

#include "stdlib.c"
#include "hm.c"
#include "AFL-2.57b/sched.h"

#include <pthread.h>
#include <sys/syscall.h>
#include <assert.h>

#define gettid() syscall(SYS_gettid)

#define RED "\33[31m"
#define OFF "\33[0m"

#define AREA ((uint8_t *)0x1B0000)

#define AFL_BITMAP_SIZE (1 << 16)
#define SCHED_BITMAP_SIZE (1 << 15)

static uint8_t *sched_bitmap = NULL;

typedef size_t (*switch_t)(const void *, const void *, size_t, bool, const void *);
typedef struct Sched (*get_sched_t)();
typedef void (*rem_t)(EventId, EventId);

static struct Sched *sched;
static size_t counter = 0;
static switch_t switch_func = NULL;
static get_sched_t get_sched_func = NULL;
static rem_t rem_func = NULL;

static Map *last_writes;
static Map *looking_for_instr;
static Map *looking_for_mem;
static Map *avoiding_instr;
static Map *obligations;
static Map *negations;

static Map *relevant_writes;
static Map *write_delay_avoiding;

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

void update(size_t hash)
{
    size_t bitindex = hash % (SCHED_BITMAP_SIZE * 8);
    size_t byteindex = bitindex / 8;
    size_t bytebitindex = bitindex % 8;
    sched_bitmap[byteindex] = sched_bitmap[byteindex] | (1 << (7 - bytebitindex));
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

static inline void check_obligations(const void *instr_addr, const void *mem_addr, size_t size, const void *func)
{
    if (ck_pos_only()) { return; }
    Vtype oblig = get(obligations, (uintptr_t)instr_addr);
    if (oblig.addrs != 0)
    {
        fprintf(stderr, "obligation reached! (%lu)\n", instr_addr);
        if (oblig.addrs != get(last_writes, (uintptr_t)mem_addr).addrs) {
            Vtype v = {.addrs = (uintptr_t)instr_addr, .tid = 0};
            insert(looking_for_instr, (uintptr_t)oblig.addrs, v);

            counter = rand() % sched->delay;
            dlcall(switch_func, instr_addr, mem_addr, size, false, func, &oblig.addrs, false);

            
        }
	// was the obligation broken or met?
        Vtype last_write = get(last_writes, (uintptr_t)mem_addr);
        if (oblig.addrs == last_write.addrs)
        {
            fprintf(stderr, "OBLIGATION MET YAHOO! %lu rf %lu\n", instr_addr, oblig.addrs);
        } else {
           fprintf(stderr, "broken ob! %lu rf %lu \n", instr_addr, last_write.addrs);
	}

        // no matter if met or broken, read will proceed after this point, so remove from
        // oblications / lookingfor and notify
        erase(obligations, (uintptr_t)instr_addr);
        erase(looking_for_instr, oblig.addrs);
        dlcall(rem_func, instr_addr, oblig.addrs);
        dont_double_switch();
    }
}

static inline void check_negations(const void *instr_addr, const void *mem_addr, size_t size, const void *func)
{
    if (ck_pos_only()) { return; }
    Vtype neg = get(negations, (uintptr_t)instr_addr);
    if (neg.addrs != 0)
    {
    	if (get(last_writes, (uintptr_t)mem_addr).addrs == neg.addrs) {
		Vtype v = {.addrs = (uintptr_t)instr_addr, .tid = 0};
		insert(avoiding_instr, instr_addr, v);
		insert(looking_for_mem, (uintptr_t)mem_addr, v);
		dlcall(switch_func, instr_addr, mem_addr, size, false, func, NULL, true); // @TODO not null -- do we care? why not NULL?
	}

        Vtype last_write = get(last_writes, (uintptr_t)mem_addr);
        if (neg.addrs != last_write.addrs)
        {
           fprintf(stderr, "NEGATION MET YAHOO! %lu rf %lu (not %lu)\n", instr_addr, last_write.addrs, neg.addrs);
        } else {
           fprintf(stderr, "broken neg! %lu rf %lu \n", instr_addr, last_write.addrs);
	}


        // no matter if met or broken, read will proceed after this point, so remove from
        // oblications / lookingfor and notify
        erase(looking_for_mem, (uintptr_t)mem_addr);
        erase(avoiding_instr, instr_addr);
        dlcall(rem_func, instr_addr, NULL);

	fprintf(stderr, "done write avoiding %lu\n", neg.addrs);
        erase(write_delay_avoiding, neg.addrs);
        dlcall(rem_func, neg.addrs, NULL);
        dont_double_switch();
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

static inline void switch_if_avoid_all_writes(const void *instr_addr, const void *mem_addr, size_t size, const void *func) {
    if (sched->avoid_uninit_writes && get(last_writes, (uintptr_t)mem_addr).addrs == 0) {
        counter = rand() % sched->delay;
        dlcall(switch_func, instr_addr, mem_addr, size, true, func, NULL, true);
        dont_double_switch();
    }
    
}

static inline void avoid_writes(const void *instr_addr, const void *mem_addr, size_t size, const void *func) {
    Vtype cause = get(relevant_writes, instr_addr);
    if (cause.addrs != 0) {
        insert(write_delay_avoiding, instr_addr, cause);
	fprintf(stderr, "avoiding %lu:write(%lu)\n", instr_addr, mem_addr);
        dlcall(switch_func, instr_addr, mem_addr, size, false, func, NULL, true); // treat as negation
	fprintf(stderr, "done write avoiding %lu:write(%lu)\n", instr_addr, mem_addr);
        erase(write_delay_avoiding, instr_addr);
        dlcall(rem_func, instr_addr, NULL);
        dont_double_switch();
    }
}

static inline void update_write_data(const void *instr_addr, const void *mem_addr, pid_t tid)
{
    if (ck_pos_only()) { return; }
    if (get(looking_for_instr, (uintptr_t)instr_addr).addrs != 0)
    {
        counter = 0;
    }

    Vtype instr_was_avoiding = get(looking_for_mem, (uintptr_t)mem_addr);
    
    if (instr_was_avoiding.addrs != 0)
    {
        erase(looking_for_mem, (uintptr_t)mem_addr);
        dlcall(rem_func, NULL, instr_was_avoiding);
        counter = 0;
    } 

    Vtype write_delay_was_avoiding = get(write_delay_avoiding, (uintptr_t)instr_addr);
    if (write_delay_was_avoiding.addrs != 0)
    {
        erase(write_delay_avoiding, (uintptr_t)instr_addr);
        dlcall(rem_func, NULL, write_delay_was_avoiding);
        counter = 0;
    } 

    Vtype v;
    v.tid = tid;
    v.addrs = (uintptr_t) instr_addr;
    insert(last_writes, (uintptr_t)mem_addr, v);
}

static inline void update_read_data(const void *instr_addr, const void *mem_addr, pid_t tid)
{
    if (ck_pos_only()) { return; }
    if (ck_rff()) { return; }

    Vtype last_write = get(last_writes, (uintptr_t) mem_addr);
    if (tid != last_write.tid) // this should capture uninit writes where tid is 0
    {
        update(hash_combine((uintptr_t)last_write.addrs, (uintptr_t)instr_addr));
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

    last_writes = create(sizeof(Entry), 1000);

    sched = (Sched*) dlcall(get_sched_func);
    fprintf(stderr, "sched rand %hu delay %hu len %hu %hu\n", sched->random_seed, sched->delay, sched->pos_len, sched->neg_len);
    srand((unsigned int)sched->random_seed);
    obligations = create(sizeof(Entry), 1000);
    relevant_writes = create(sizeof(Entry), 1000);
    for (int i = 0; i < sched->pos_len; i += 1)
    {
        // @TODO remove additional field in obligation map; low priority
        Vtype v = {.addrs = sched->obligations[i].w, .tid = -1};
        insert(obligations, sched->obligations[i].r, v);
        Vtype vr = {.addrs = sched->obligations[i].r, .tid = -1};
        insert(relevant_writes, sched->obligations[i].w, vr);
    }
    negations = create(sizeof(Entry), 1000);
    for (int i = sched->pos_len; i < sched->neg_len + sched->pos_len; i += 1)
    {
        // @TODO remove additional field in obligation map; low priority
        Vtype v = {.addrs = sched->obligations[i].w, .tid = -1};
        insert(negations, sched->obligations[i].r, v);
        Vtype vr = {.addrs = sched->obligations[i].r, .tid = -1};
        insert(relevant_writes, sched->obligations[i].w, vr);
    }

    looking_for_instr = create(sizeof(Entry), 10);
    looking_for_mem = create(sizeof(Entry), 10);
    avoiding_instr = create(sizeof(Entry), 10);
    write_delay_avoiding = create(sizeof(Entry), 10);

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
