#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <optional>

#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <pthread.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/shm.h>

#include "AFL-2.57b/sched.h"

#include <string>
#include <fstream>

#include <nlohmann/json.hpp>

#include <stdatomic.h>
#include <execinfo.h>

#define MAX_BT_LENGTH 10

#define error(msg, ...)                                             \
    do                                                              \
    {                                                               \
        fprintf(stderr, "\33[31merror\33[0m: %d: %s:%u: " msg "\n", \
                my_gettid(), __FILE__, __LINE__, ##__VA_ARGS__);    \
        abort();                                                    \
    } while (false)
#define warning(msg, ...)                                             \
    do                                                                \
    {                                                                 \
        fprintf(stderr, "\33[33mwarning\33[0m: %d: %s:%u: " msg "\n", \
                my_gettid(), __FILE__, __LINE__, ##__VA_ARGS__);      \
    } while (false)
#define debug(msg, ...)                                                                                                    \
    do                                                                                                                     \
    {                                                                                                                      \
        fprintf(stderr, "\33[36mdebug\33[0m: %d: " msg "\n", my_gettid(), \
                ##__VA_ARGS__);                                                                                            \
    } while (false)
#undef debug
#define debug(msg, ...) \
    do                  \
    {                   \
    } while (false)

static pid_t my_gettid(void)
{
    register pid_t tid asm("eax");
    // Warning: this assumes the thread ID is stored at %fs:0x2d0.
    asm volatile(
        "mov %%fs:0x2d0,%0\n"
        : "=r"(tid));
    return tid;
}


int THREAD_AFFINITY = 0;

#define PRIORITY_MAX 10000
#define SCHED_MAP_SIZE (1 << 15)
#define MAP_SIZE (1 << 16)
static uint8_t *sched_bitmap = NULL;

static uint8_t *pthread_create_shm;
static uint8_t placeholder_pthread_create_shm = 0;
static bool placeholder_log_indicator_shm = 0;
static bool *log_indicator_shm;

void logTrace(std::string type, void *ptr, int thId = -1, bool flag = false, size_t size = 0, uintptr_t = 0);
char* to_log; 
char* log_dir;

void logTraceForkJoin(std::string type, pthread_t *ptr);
void logThreadMapping();

struct Event
{
    const void *event_id;
    const void *addr;
    const void *func;
    size_t size;
    bool is_write;
};

Event uninit() {
	Event e;
        e.event_id = 0;
        e.addr = 0;
        e.func = 0;
        e.size = 1;
        e.is_write = true;
	return e;
}

void update(size_t hash)
{
    size_t bitindex = hash % (SCHED_MAP_SIZE * 8);
    size_t byteindex = bitindex / 8;
    size_t bytebitindex = bitindex % 8;
    sched_bitmap[byteindex] = sched_bitmap[byteindex] | (1 << (7 - bytebitindex));
}

void rem(size_t hash)
{
    size_t bitindex = hash % (SCHED_MAP_SIZE * 8);
    size_t byteindex = bitindex / 8;
    size_t bytebitindex = bitindex % 8;
    sched_bitmap[byteindex] = sched_bitmap[byteindex] & (~(1 << (7 - bytebitindex)));
}

inline size_t hash_combine(uintptr_t lhs,  uintptr_t rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}


inline uintptr_t get_context_hash() {
    void ** bt = (void**) malloc(sizeof(void*) * MAX_BT_LENGTH);
    int len = backtrace(bt, MAX_BT_LENGTH);
    // @TODO not stable across runs without ASLR -- switch to e9patch instrum
    uintptr_t hash = (uintptr_t) bt[0];
    int i = 1;
    while (i < len) {
        hash = hash_combine(hash, (uintptr_t) bt[i]);
        i += 1;
    }
    return hash;
}

enum ObligState { PosReachR, PosExctdW, PosNoInfo, NegExctdW, NegReachW, NegOtherW, NegNoInfo };

struct Obligation {
    ReadFrom rf;
    bool is_negation;
    ObligState state;
    int count;
    uintptr_t last_write;
};

// static std::set<std::pair<uintptr_t, uintptr_t>> rfs_d;
static std::unordered_map<uintptr_t, EventId> last_writes;

static std::unordered_map<EventId, std::unordered_set<Obligation*>> relevant_obs;
static std::vector<Obligation*> obs_to_visit;
static std::unordered_map<uintptr_t, std::unordered_set<Obligation*>> mem_relevant_obs;

static std::unordered_map<EventId, std::unordered_set<Obligation*>> avoiding;
static std::unordered_map<EventId, std::unordered_set<Obligation*>> looking_for;
static std::unordered_map<EventId, std::unordered_set<Obligation*>> avoiding_mem;
static std::unordered_map<EventId, std::unordered_set<Obligation*>> looking_for_mem;

static std::unordered_map<uintptr_t, EventId> all_last_writes;
static std::unordered_map<uintptr_t, pid_t> all_last_write_threads; // TODO hash of pair

struct MyHash {
    size_t operator()(std::pair<EventId, EventId> const&  p) const {
        return hash_combine(p.first, p.second);
    }
};
static std::unordered_set<std::pair<EventId, EventId>, MyHash> all_rfs;

bool RECORD_EXACT_RFS = false;

extern "C"
{
    void remove_pending_obligation(const void *r, const void *w)
    {
      return;
    }
}

/****************************************************************************/
/* SCHEDULE INTERPRETER                                                     */
/****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static bool sched_inited = false;
void update_obs(Event, bool);

static Sched *rf_sched;
using json = nlohmann::json;

extern "C" Sched* read_json_sched(const char*);
Sched* parse_sched(const char *filename)
{

    if (filename == nullptr)
    {
        rf_sched = (Sched *)malloc(sizeof(Sched));
        rf_sched->random_seed = 0;
        rf_sched->delay = 1;
        rf_sched->dist = 0;
        rf_sched->pos_len = 0;
        rf_sched->neg_len = 0;
        rf_sched->obligations = nullptr;
        return rf_sched;
    }
    uint8_t *sched;
    // Sched *new_sched = read_sched(filename);
    //
    
    const char *json_sched= getenv("JSON_SCHEDULE");
    if (json_sched != nullptr && atoi(json_sched) != 0) {
        std::ifstream f (filename);
        json data = json::parse(f);
        rf_sched = (Sched *)malloc(sizeof(Sched));
        rf_sched->random_seed = data["random_seed"];
        rf_sched->delay = data["delay"];
        rf_sched->dist = data["dist"];
        rf_sched->pos_len = data["pos_len"];
        rf_sched->neg_len = data["neg_len"];
        rf_sched->avoid_uninit_writes =  data["avoid_uninit_writes"];
	if (rf_sched->pos_len + rf_sched->neg_len == 0) {
	    rf_sched->obligations = nullptr;
	    return rf_sched;
	}

	int i = 0;
	ReadFrom* rfs = (ReadFrom*) malloc(sizeof(ReadFrom) * data["obligations"].size());
	for (auto o : data["obligations"]) {
	    rfs[i].r = o["r"];
	    rfs[i].w = o["w"];
	    i+=1;
	}
	rf_sched->obligations = rfs;
	return rf_sched;
    }
    
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
        error("failed to open file \"%s\": %s", filename, strerror(errno));
    struct stat buf;
    if (fstat(fd, &buf) != 0)
        error("failed to stat file \"%s\": %s", filename, strerror(errno));
    size_t size = buf.st_size;
    if (size > BUFSIZ)
        size = BUFSIZ;
    sched = (uint8_t *)malloc(size);
    if (sched == NULL)
        error("failed to allocated %zu bytes: %s", size, strerror(errno));
    for (size_t i = 0; i < size;)
    {
        ssize_t r = read(fd, sched, size);
        if (r == 0)
            error("failed to read file \"%s\": EOF", filename);
        else if (r < 0)
            error("failed to read file \"%s\": %s", filename, strerror(errno));
        i += r;
    }

    close(fd);

    rf_sched = (Sched *)sched;
    rf_sched->obligations = (ReadFrom *)(sched + sizeof(Sched));

    debug("Positives \n");
    for (int i = 0; i < rf_sched->pos_len; i += 1)
    {
        debug("(%lp,%lp),", rf_sched->obligations[i].r, rf_sched->obligations[i].w);
    }
    debug("\nNegatives\n");
    for (int i = rf_sched->pos_len; i < rf_sched->pos_len + rf_sched->neg_len; i += 1)
    {
        debug("(%lp,%lp),", rf_sched->obligations[i].r, rf_sched->obligations[i].w);
    }
    debug("\n");

    return rf_sched;
}

static uint8_t *bitmap = NULL;

uint32_t TIMER_MAX = 10000;
bool NO_POS = false;
bool POS_ONLY = false;
bool NO_RFF = false;
bool ALL_RFF = false;
uint16_t PRIORITY_CHANGE_FREQ_PCT = 100;

void add_lf_av(Obligation* ob, Event e);

FILE * exact_rfs_file = nullptr;

static void sched_initialize(void)
{
    if (sched_inited)
        return;
    sched_inited = true;

    const char *no_pos= getenv("NO_POS");
    if (no_pos && strcmp(no_pos, "0")) {
        NO_POS = true;
    }

    const char *timer_max = getenv("TIMER_MAX");
    if (timer_max) {
        TIMER_MAX = atoi(timer_max);
    } 

    const char *all_rff= getenv("ALL_RFF");
    if (all_rff && strcmp(all_rff, "0")) {
        ALL_RFF = true;
    }

    const char *ta = getenv("THREAD_AFFINITY");
    if (ta) {
        THREAD_AFFINITY = atoi(ta);
        assert(std::abs(THREAD_AFFINITY) < PRIORITY_MAX);
    }

    const char *pos_only= getenv("POS_ONLY");
    if (pos_only && strcmp(pos_only, "0")) {
        POS_ONLY = true;
    }

    const char *no_rff= getenv("NO_RFF");
    if (no_rff && strcmp(no_rff, "0")) {
        NO_RFF = true;
    }

    const char *pcfp = getenv("PRIORITY_CHANGE_FREQ_PCT");
    if (pcfp) {
        PRIORITY_CHANGE_FREQ_PCT = atoi(pcfp);
    }

    const char *record_exact_rfs= getenv("RECORD_EXACT_RFS");
    if (record_exact_rfs && strcmp(record_exact_rfs, "0")) {
        RECORD_EXACT_RFS = true;
        exact_rfs_file = fopen("exact_rfs.csv", "a+");
        assert(exact_rfs_file != nullptr);
    }

    const char *shm_id_env = getenv("__AFL_SHM_ID");
    if (shm_id_env == nullptr)
    {
        pthread_create_shm = &placeholder_pthread_create_shm;
        log_indicator_shm = &placeholder_log_indicator_shm;
        bitmap = (uint8_t *)malloc(MAP_SIZE);
        sched_bitmap = (uint8_t *)malloc(SCHED_MAP_SIZE);
    }
    else
    {
        int shm_id = atoi(shm_id_env);
        uint8_t* base =  ((uint8_t *)shmat(shm_id, NULL, 0));      
        sched_bitmap =  base + MAP_SIZE;
        pthread_create_shm = base + MAP_SIZE + SCHED_MAP_SIZE;
        log_indicator_shm = (bool*) base + MAP_SIZE + SCHED_MAP_SIZE + 1;
        bitmap = (uint8_t *)shmat(shm_id, NULL, 0);
    }

    const char *sched_name = getenv("SCHEDULE");
    parse_sched(sched_name);
    Obligation* obs = (Obligation*) malloc((rf_sched->pos_len + rf_sched->neg_len)*sizeof(Obligation));
    for (int i = 0; i < rf_sched->pos_len; i++) {
        obs[i].rf = rf_sched->obligations[i];
        obs[i].is_negation = false;
        obs[i].count = 1;
        obs[i].state = PosNoInfo;
        obs[i].last_write = 0;

        relevant_obs[obs[i].rf.r].insert(&obs[i]); 
        relevant_obs[obs[i].rf.w].insert(&obs[i]);         
    }
    for (int i = rf_sched->pos_len; i < rf_sched->pos_len + rf_sched->neg_len; i++) {
        obs[i].rf = rf_sched->obligations[i];
        obs[i].is_negation = true;
        obs[i].count = -1;
        obs[i].state = NegOtherW;
        obs[i].last_write = 0;

        if (!NO_RFF && !ALL_RFF) {
            update(hash_combine(obs[i].rf.r, obs[i].rf.w + 1));
        }

        if (!POS_ONLY) {
            add_lf_av(&obs[i], uninit());
        }

        relevant_obs[obs[i].rf.r].insert(&obs[i]); 
        relevant_obs[obs[i].rf.w].insert(&obs[i]);         
    }
    srand(rf_sched->random_seed);

}

/****************************************************************************/
/* PTHREADS INTERCEPT                                                       */
/****************************************************************************/

typedef int (*pthread_mutex_lock_t)(pthread_mutex_t *);
typedef int (*pthread_mutex_trylock_t)(pthread_mutex_t *);
typedef int (*pthread_mutex_unlock_t)(pthread_mutex_t *);
typedef int (*pthread_create_t)(pthread_t *, const pthread_attr_t *,
                                void *(*)(void *), void *);
typedef int (*pthread_exit_t)(void *);
typedef int (*pthread_cond_wait_t)(pthread_cond_t *, pthread_mutex_t *);
typedef int (*pthread_cond_timedwait_t)(pthread_cond_t *, pthread_mutex_t *,
                                        const struct timespec *);
typedef int (*pthread_cond_signal_t)(pthread_cond_t *);
typedef int (*pthread_cond_broadcast_t)(pthread_cond_t *);
typedef void *(*libc_malloc_t)(size_t size);
typedef void (*libc_free_t)(void *ptr);

static pthread_mutex_lock_t libc_pthread_mutex_lock = nullptr;
static pthread_mutex_trylock_t libc_pthread_mutex_trylock = nullptr;
static pthread_mutex_unlock_t libc_pthread_mutex_unlock = nullptr;
static pthread_create_t libc_pthread_create = nullptr;
static pthread_exit_t libc_pthread_exit = nullptr;
static pthread_cond_wait_t libc_pthread_cond_wait = nullptr;
static pthread_cond_timedwait_t libc_pthread_cond_timedwait = nullptr;
static pthread_cond_signal_t libc_pthread_cond_signal = nullptr;
static pthread_cond_broadcast_t libc_pthread_cond_broadcast = nullptr;

static void *(*libc_malloc)(size_t size);
static void (*libc_free)(void *ptr);

#define libc_pthread_mutex_init pthread_mutex_init
#define libc_pthread_mutex_destroy pthread_mutex_destroy
#define libc_pthread_cond_init pthread_cond_init
#define libc_pthread_cond_destroy pthread_cond_destroy



/*
 * Thread structure.
 */
struct THREAD
{
    pid_t tid = my_gettid();
    pthread_t self = pthread_self();
    bool awake = true;
    bool alive = true;
    bool stop_on_next = false;
    char color = '1';
    pthread_cond_t wake_cond = PTHREAD_COND_INITIALIZER;
    void *retval = nullptr;
    std::optional<Event> next_event = std::nullopt;
    std::vector<THREAD *> waiters;
    int pos_priority = -1;
};

/*
 * Mutex structure.
 */
struct MUTEX
{
    pid_t owner;

    // Note: by default we assume that all mutexes are recusrive, so do not
    // deadlock when the same thread attempts to reacquire the mutex:
    size_t count = 1;

    std::vector<THREAD *> waiters;
};
typedef std::map<pthread_mutex_t *, MUTEX> MUTEXES;

/*
 * Conditional variable structure.
 */
struct CONDVAR
{
    pthread_mutex_t *mutex = nullptr;
    std::vector<THREAD *> waiters;
};
typedef std::map<pthread_cond_t *, CONDVAR> CONDVARS;

/*
 * DATA for pthread_create() parameters.
 */
typedef void *(*start_routine_t)(void *);
struct DATA
{
    start_routine_t start_routine;
    void *arg;
    pthread_cond_t start;
};

/*
 * Global state.
 */
thread_local THREAD t; // This own thread (note: thread_local)
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static MUTEXES *mutexes = nullptr;
static CONDVARS *condvars = nullptr;
static std::vector<THREAD *> *threads = nullptr;      // All threads
static std::map<pthread_t, THREAD *> *tidx = nullptr; // All threads
static pid_t run;                                     // Only this thread may run
static char color = '1';

bool depends(std::optional<Event> a, std::optional<Event> b)
{
    if (!a.has_value() || !b.has_value() || (!a->is_write && !b->is_write))
    {
        return false;
    }

    auto a_addr = (char *)a.value().addr;
    auto a_size = a.value().size;
    auto b_addr = (char *)b.value().addr;
    auto b_size = b.value().size;
    if ((a_addr <= b_addr && b_addr <= a_addr + a_size) ||
        (b_addr <= a_addr && a_addr <= b_addr + b_size))
    {
        return true;
    }
    return false;
}


pthread_t timer_thread;

atomic_bool timer_accessed;

static void signal(void);
static void record_rfs_if_needed(Event);

static void *timer_thread_fn(void *arg) {
    while(true) {
        usleep(TIMER_MAX);
        bool was_accessed = atomic_exchange(&timer_accessed, false);
        if (!was_accessed) {
            libc_pthread_mutex_lock(&global_mutex);
            for (auto thread : *threads) {
                if (thread->tid == run) {
                    debug("BLOCKING ALERT: culprit %d", thread->tid);
                    thread->stop_on_next = true;
                    signal();
                    break;
                }
            }

            libc_pthread_mutex_unlock(&global_mutex);
        }
    }
}


/*
 * Initialize this library.
 */
static void initialize(void)
{
    static bool inited = false;
    if (inited)
        return;
    inited = true;

    libc_malloc = (libc_malloc_t)dlsym(RTLD_NEXT, "malloc");
    libc_free = (libc_free_t)dlsym(RTLD_NEXT, "free");

    pthread_mutex_init(&global_mutex, NULL);
    mutexes = new MUTEXES;
    condvars = new CONDVARS;
    threads = new std::vector<THREAD *>;
    tidx = new std::map<pthread_t, THREAD *>;

    libc_pthread_create = (pthread_create_t)dlsym(RTLD_NEXT, "pthread_create");
    libc_pthread_exit = (pthread_exit_t)dlsym(RTLD_NEXT, "pthread_exit");
    libc_pthread_mutex_lock = (pthread_mutex_lock_t)
        dlsym(RTLD_NEXT, "pthread_mutex_lock");
    libc_pthread_mutex_trylock = (pthread_mutex_trylock_t)
        dlsym(RTLD_NEXT, "pthread_mutex_trylock");
    libc_pthread_mutex_unlock = (pthread_mutex_unlock_t)
        dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    // Note: have to use specific version to work-around a libdl bug.
    //       This will likely break in other versions...
    libc_pthread_cond_wait = (pthread_cond_wait_t)
        dlvsym(RTLD_NEXT, "pthread_cond_wait", "GLIBC_2.3.2");
    libc_pthread_cond_timedwait = (pthread_cond_timedwait_t)
        dlvsym(RTLD_NEXT, "pthread_cond_timedwait", "GLIBC_2.3.2");
    libc_pthread_cond_signal = (pthread_cond_signal_t)
        dlvsym(RTLD_NEXT, "pthread_cond_signal", "GLIBC_2.3.2");
    libc_pthread_cond_broadcast = (pthread_cond_broadcast_t)
        dlvsym(RTLD_NEXT, "pthread_cond_broadcast", "GLIBC_2.3.2");
    if (libc_pthread_create == nullptr ||
        libc_pthread_mutex_lock == nullptr ||
        libc_pthread_mutex_unlock == nullptr ||
        libc_pthread_cond_wait == nullptr ||
        libc_pthread_cond_timedwait == nullptr ||
        libc_pthread_cond_signal == nullptr ||
        libc_pthread_cond_broadcast == nullptr)
        error("failed to find pthread function(s)");

    threads->push_back(&t);
    tidx->insert({t.self, &t});

    sched_initialize();
    run = t.tid;

    to_log = getenv("TO_LOG");
    log_dir = getenv("LOG_DIR_MAP");
    if (*log_indicator_shm) {
        to_log = "true";
        log_dir = "mapping.log";
        FILE *file = nullptr;
        file = fopen("events.log", "w");
        fclose(file);
    }

    if (to_log != nullptr && log_dir != nullptr && strcmp(to_log, "true") == 0)
    {
        FILE *file = nullptr;
        file = fopen(log_dir, "a+");
        if (file != nullptr)
        {
            fprintf(file, "init_pthread(%lu) = tid(%d) \n", pthread_self(), gettid());
            fclose(file);
        }
        else
        {
            warning("file = nullptr, please check whether the file/dir has been created");
        }
    }

    
    int r = libc_pthread_create(&timer_thread, nullptr, timer_thread_fn, nullptr);
}

static inline bool should_take(THREAD *tt)
{
    if (tt->next_event.has_value()) { 
        auto e = tt->next_event.value();    
        return (looking_for.find((EventId)e.event_id) != looking_for.end()
            || looking_for_mem.find((uintptr_t)e.addr) != looking_for.end());
    } else { return false; }
}

static inline bool should_avoid(THREAD *tt)
{

    if (tt->next_event.has_value()) { 
        auto e = tt->next_event.value();    
        return (avoiding.find((EventId)e.event_id) != avoiding.end()
            || avoiding_mem.find((uintptr_t)e.event_id) != avoiding_mem.end());
    } else { return false; }
}

uint16_t starvation_limit;


/*
 * Signal the next thread to run.
 */
static void signal(void)
{
    atomic_exchange(&timer_accessed, true);

    if (t.stop_on_next) {
        t.pos_priority = -1; // reset priority now that unblocked
        t.stop_on_next = false;
        return; // don't signal since another thread is running
    }

    size_t nthreads = threads->size();
    THREAD *thread = nullptr;

    int best_pos_priority = -1;
    THREAD *best_pos_thread = nullptr;
    int num_awake = 0;
    for (auto item : *threads)
    {
        if (item->awake)
        {

            bool avoid = should_avoid(item);
            bool take = should_take(item);
            num_awake += 1;

            if (item->pos_priority < 0)
            {
                item->pos_priority = rand() % PRIORITY_MAX;
            }

            int mod_priority;
            if (avoid && !take) {
                mod_priority = item->pos_priority + PRIORITY_MAX;
            } else if (take && !avoid) {
                mod_priority = item->pos_priority + 3*PRIORITY_MAX;
            } else if (NO_POS) {
                // stay on current thread if possible
                mod_priority = item->pos_priority + 2*PRIORITY_MAX;
                if (&t == item) {
                    mod_priority = 3*PRIORITY_MAX-1;
                }
            } else if (item->stop_on_next) {
                mod_priority = item->pos_priority;
            } else {
                mod_priority = item->pos_priority + 2*PRIORITY_MAX;
            }

            if (item == &t) {
                mod_priority += THREAD_AFFINITY;
            }


            if (mod_priority > best_pos_priority)
            {
                best_pos_thread = item;
                best_pos_priority = mod_priority;
            }
        }
    }

    if (num_awake == 0)
    {
        error("DEADLOCK");
        abort();
    }

    thread = best_pos_thread;
    
    for (auto item : *threads)
    {
        if (item->awake && item->tid != thread->tid && depends(thread->next_event, item->next_event))
        {
            if (PRIORITY_CHANGE_FREQ_PCT < 100) {
                if (rand() % 100 < PRIORITY_CHANGE_FREQ_PCT) {
                    item->pos_priority = -1;
                }
            } else {
                item->pos_priority = -1;
            }
        }
    }

    if (PRIORITY_CHANGE_FREQ_PCT < 100) {
        if (rand() % 100 < PRIORITY_CHANGE_FREQ_PCT) {
            thread->pos_priority = -1;
        }
    } else {
        thread->pos_priority = -1;
    }

    run = thread->tid;
    if (&t != thread) {
        libc_pthread_cond_signal(&thread->wake_cond);
        starvation_limit = 1; // Note: won't help with multi thread starvation, need count per thread (TODO)
    } else {
        if (starvation_limit < 1) {
            for (auto item : *threads) {
                item->pos_priority = rand() % 10000;
            }
        }
        starvation_limit += 1;
    }
}




/*
 * Wait until the current thread can run.
 */
static bool wait(const struct timespec *abstime)
{
    bool timeout = false;
    while (!t.awake || run != t.tid)
    {
        if (abstime != nullptr)
        {
            int r = libc_pthread_cond_timedwait(&t.wake_cond, &global_mutex,
                                                abstime);
            if (r == ETIMEDOUT)
            {
                timeout = t.awake = true;
                abstime = nullptr;
            }
        }
        else
            int r = libc_pthread_cond_wait(&t.wake_cond, &global_mutex);
    }
    return timeout;
}

/*
 * Suspend the current thread.
 */
static bool suspend(const struct timespec *abstime, const char *reason)
{
    debug("WAIT (%s)%s", reason, (abstime != nullptr ? " [with timeout]" : ""));
    if (!t.awake)
        error("thread already suspended");
    t.awake = false;

    // Find the next thread to run:
    signal();

    // Wait until our turn to run:
    bool r = wait(abstime);

    debug("WAKE (%s)", reason);
    return r;
}

/*
 * Wake the given thread.
 */
static void wake(THREAD *thread)
{
    if (thread->awake)
        error("thread already awake (tid=%d,thread=%d)", t.tid, thread->tid);
    thread->awake = true;
    debug("wake(%d)", thread->tid);
}

/*
 * Mutex lock operation.
 */
static int do_pthread_mutex_lock(pthread_mutex_t *mutex, THREAD *thread,
                                 const char *reason)
{
    debug("\33[36mdo_pthread_mutex_lock\33[0m(%p, %d) [%s]", mutex,
          thread->tid, reason);
    MUTEX empty;
    empty.owner = thread->tid;
    auto i = mutexes->insert({mutex, empty});
    if (i.second)
        return 0; // Acquired

    // Mutex already locked:
    MUTEX *m = &i.first->second;
    if (m->owner < 0)
    {
        m->owner = thread->tid;
        m->count = 1;
        return 0; // Acquired
    }
    if (m->owner == thread->tid)
    {
        // The same thread attempts to reacquire the mutex:
        m->count++;
        return 0;
    }
    // Wait for the mutex to be unlocked:
    m->waiters.push_back(thread);
    return EBUSY;
}

/*
 * Mutex unlock operation.
 */
static int do_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    debug("\33[33mdo_pthread_mutex_unlock\33[0m(%p)", mutex);
    auto i = mutexes->find(mutex);
    if (i == mutexes->end())
    {
        warning("no such mutex (tid=%d,mutex=%p)", t.tid, mutex);
        return EINVAL;
    }
    MUTEX *m = &i->second;
    if (m->owner != t.tid)
    {
        error("non-owner attempt to unlock mutex (tid=%d,owner=%d,mutex=%p)",
              t.tid, m->owner, mutex);
        return EINVAL;
    }
    if (m->count > 1)
    {
        // The mutex was locked more than once:
        m->count--;
        return 0;
    }
    if (m->waiters.size() == 0)
    {
        // No waiters:
        mutexes->erase(i);
        return 0;
    }

    auto to_wake = rand() % m->waiters.size();
    THREAD *thread = m->waiters[to_wake];
    m->waiters[to_wake] = m->waiters.back();
    m->waiters.pop_back();
    m->owner = -1; // thread->tid;
    wake(thread);

    return 0;
}


int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    initialize();
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;

    uintptr_t ctxt_hash = get_context_hash();

    Event e;
    e.event_id = (void*) ctxt_hash;
    e.addr = mutex;
    e.func = (void*) ctxt_hash;
    e.size = 1;
    e.is_write = false;
    t.next_event = std::optional<Event>{e};

    if (!POS_ONLY) {
        update_obs(e, false);
    }

    signal();
    wait(nullptr);

    if (!POS_ONLY) {
        update_obs(e, true);
    }

    record_rfs_if_needed(e);

    int r = 0;
    while (true)
    {
        r = do_pthread_mutex_lock(mutex, &t, "pthread_mutex_lock");
        debug("pthread_mutex_lock(%p) = %d (%s)", mutex, r, strerror(r));
        if (r != EBUSY)
            break;
        suspend(nullptr, "mutex already locked");
    }
    libc_pthread_mutex_unlock(&global_mutex);

    // log
    logTrace("acq", mutex, -1, false, 0, (uintptr_t) e.event_id);

    return r;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    initialize();
    MUTEX empty;
    empty.owner = t.tid;
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;


    uintptr_t ctxt_hash = get_context_hash();

    Event e;
    e.event_id = (void*) ctxt_hash;
    e.addr = mutex;
    e.func = (void*) ctxt_hash;
    e.size = 1;
    e.is_write = false;
    t.next_event = std::optional<Event>{e};

    if (!POS_ONLY) {
        update_obs(e, false);
    }

    signal();
    wait(nullptr);


    debug("pthread_mutex_trylock(%p)", mutex);
    auto i = mutexes->insert({mutex, empty});
    int r = 0;
    if (!i.second)
    {
        MUTEX *m = &i.first->second;
        if (m->owner < 0)
        {
            m->owner = t.tid;
            m->count = 1;
        }
        else if (m->owner == t.tid)
            m->count++;
        else
            r = EBUSY;
    }

    // log only if successes
    if (r) {
        if (!POS_ONLY) {
            update_obs(e, true);
        }

        logTrace("acq", mutex, -1, false, 0, (uintptr_t) e.event_id);
    }

    record_rfs_if_needed(e);

    libc_pthread_mutex_unlock(&global_mutex);

    return r;
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex)
{
    error("pthread_mutex_timedlock: NYI");
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    initialize();
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;

    uintptr_t ctxt_hash = get_context_hash();

    Event e;
    e.event_id = (void*) ctxt_hash;
    e.addr = mutex;
    e.func = (void*) ctxt_hash;
    e.size = 1;
    e.is_write = true;
    t.next_event = std::optional<Event>{e};

    if (!POS_ONLY) {
        update_obs(e, false);
    }

    signal();
    wait(nullptr);

    if (!POS_ONLY) {
        update_obs(e, true);
    }

    int r = do_pthread_mutex_unlock(mutex);
    debug("pthread_mutex_unlock(%p) = %d (%s)", mutex, r, strerror(r));
    
    // log
    logTrace("rel", mutex, -1, false, 0, (uintptr_t) e.event_id);

    signal();
    wait(nullptr);

    record_rfs_if_needed(e);

    libc_pthread_mutex_unlock(&global_mutex);

    return r;
}

/*
 * Condition variable wait operation.
 */
static int do_pthread_cond_timedwait(pthread_cond_t *cond,
                                     pthread_mutex_t *mutex,
                                     const struct timespec *abstime)
{
    initialize();
    CONDVAR empty;
    libc_pthread_mutex_lock(&global_mutex);

    signal();
    wait(nullptr);
    
    int r = do_pthread_mutex_unlock(mutex);
    if (r != 0)
    {
        debug("pthread_cond_wait(%p, %p) = %d (%s)", cond, mutex, r, strerror(r));
        libc_pthread_mutex_unlock(&global_mutex);
        return r;
    }
    // time out immediately for timed waits
    if (abstime != nullptr) {
        r = do_pthread_mutex_lock(mutex, &t, "pthread_cond_wait");
        r = ETIMEDOUT;
        debug("pthread_cond_time_wait timed out");
        libc_pthread_mutex_unlock(&global_mutex);
        return r;
    }

    auto i = condvars->insert({cond, empty});
    CONDVAR *c = &i.first->second;
    c->waiters.push_back(&t);
    if (c->mutex == nullptr)
        c->mutex = mutex;
    if (c->mutex != mutex)
        error("mismatching condvar mutex: %p vs %p", c->mutex, mutex);

    debug("pthread_cond_wait(%p, %p) [SUSPEND]", cond, mutex);
    bool timeout = suspend(abstime, "condition variable wait");

    // Re-acquire the mutex:
    while (true)
    {
        r = do_pthread_mutex_lock(mutex, &t, "pthread_cond_wait");
        if (r != EBUSY)
            break;
        suspend(nullptr, "condition variable; mutex already locked");
    }
    if (r == 0 && timeout)
        r = ETIMEDOUT;
    debug("pthread_cond_wait(%p, %p) = %d (%s)", cond, mutex, r, strerror(r));
    libc_pthread_mutex_unlock(&global_mutex);
    return r;
}

int pthread_cond_timedwait(pthread_cond_t *cond,
                           pthread_mutex_t *mutex,
                           const struct timespec *abstime)
{
    int r = do_pthread_cond_timedwait(cond, mutex, abstime);
    return r;
}

int pthread_cond_wait(pthread_cond_t *cond,
                      pthread_mutex_t *mutex)
{
    logTrace("wait_sleep", cond);

    int r = do_pthread_cond_timedwait(cond, mutex, nullptr);

    return r;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    initialize();
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;
    auto i = condvars->find(cond);
    if (i == condvars->end())
    {
        // No waiters
        int r = 0;
        debug("pthread_cond_signal(%p) = %d (%s) [NO WAITERS]", cond, r,
              strerror(r));
        libc_pthread_mutex_unlock(&global_mutex);
        return r;
    }
    CONDVAR *c = &i->second;

    auto to_wake = rand() % c->waiters.size();
    c->waiters[to_wake];

    THREAD *thread = c->waiters[to_wake];
    c->waiters[to_wake] = c->waiters.back();
    c->waiters.pop_back();
    if (c->waiters.size() == 0)
        condvars->erase(i);
    
    logTrace("signal", cond, thread->tid, false);

    logTrace("wait_wake", cond, thread->tid, true);
    
    wake(thread);
    int r = 0;
    debug("pthread_cond_signal(%p) = %d (%s) [WAKE=%d]", cond, r, strerror(r),
          thread->tid);
    libc_pthread_mutex_unlock(&global_mutex);

    return r;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    initialize();
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;
    auto i = condvars->find(cond);
    if (i == condvars->end())
    {
        // No waiters
        int r = 0;
        debug("pthread_cond_broadcast(%p) = %d (%s) [NO WAITERS]", cond, r,
              strerror(r));
        libc_pthread_mutex_unlock(&global_mutex);
        return r;
    }
    CONDVAR *c = &i->second;
    int r = 0;

    // log
    logTrace("sigAll", cond);

    for (auto *thread : c->waiters)
    {
        debug("pthread_cond_broadcast(%p) = %d (%s) [WAKE %d]", cond, r,
              strerror(r), thread->tid);
        wake(thread);

        // log
        logTrace("wait_wake", cond, thread->tid, true);
    }
    condvars->erase(i);
    libc_pthread_mutex_unlock(&global_mutex);

    return r;
}

static void thread_exit_wrapper(void *arg)
{
    libc_pthread_mutex_lock(&global_mutex);
    tidx->erase(t.self);
    size_t nthreads = threads->size();
    for (size_t i = 0; i < nthreads; i++)
    {
        if (threads->at(i) == &t)
        {
            threads->at(i) = threads->back();
            threads->pop_back();
            break;
        }
    }
    for (auto *thread : t.waiters)
    {
        thread->retval = t.retval;
        wake(thread);
        int r = 0;
        debug("pthread_exit(%p) = %d (%s) [WAKE %d]", &t, r, strerror(r),
              thread->tid);
    }
    debug("pthread_terminate(%p)", (void *)t.self);
    t.alive = false;
    signal();
    libc_pthread_mutex_unlock(&global_mutex);
}

static void *thread_start_wrapper(void *arg_0)
{
    DATA *data = (DATA *)arg_0;
    void *arg = data->arg;
    start_routine_t start_routine = data->start_routine;

    libc_pthread_mutex_lock(&global_mutex);
    color++;
    switch (color)
    {
    case '7':
        color = '1';
        break;
    case '4':
        color++;
        break;
    }
    t.color = color;
    threads->push_back(&t);
    tidx->insert({t.self, &t});
    t.awake = true;
    debug("pthread_start(%p)", (void *)t.self);
    logThreadMapping();
    libc_pthread_cond_signal(&data->start);
    wait(nullptr);
    libc_pthread_mutex_unlock(&global_mutex);

    pthread_cleanup_push(thread_exit_wrapper, nullptr);
    t.retval = start_routine(arg);
    pthread_cleanup_pop(/*execute=*/true);

    return t.retval;
}

int sched_yield() {
    libc_pthread_mutex_lock(&global_mutex);
    signal();
    wait(nullptr);    
    libc_pthread_mutex_unlock(&global_mutex);
    return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    initialize();

    DATA *data = new DATA;
    data->start_routine = start_routine;
    data->arg = arg;
    pthread_cond_init(&data->start, nullptr);

    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;
    int r = libc_pthread_create(thread, attr, thread_start_wrapper, data);
    libc_pthread_cond_wait(&data->start, &global_mutex);
    // log on successful creation of thread
    if (r == 0)
        logTraceForkJoin("fork", thread);

    debug("pthread_create(%p, %p, %p) = %d (%s)", (void *)*thread, start_routine, arg,
          r, strerror(r));
    delete data;

    signal();
    wait(nullptr);

    libc_pthread_mutex_unlock(&global_mutex);

    return r;
}

int pthread_join(pthread_t pthread, void **retval)
{
    initialize();
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;
    auto i = tidx->find(pthread);
    int r = 0;
    if (i == tidx->end())
    {
        debug("pthread_join(%p) = %d (%s) [ALREADY EXITTED]", (void *)pthread, r, strerror(r));
        libc_pthread_mutex_unlock(&global_mutex);
        return r;
    }
    THREAD *thread = i->second;
    thread->waiters.push_back(&t);
    debug("pthread_join(%p) = %d (%s)", (void *)pthread, r, strerror(r));
    suspend(nullptr, "waiting for join");
    debug("pthread_join(%p) = %d (%s) [WAKE]", (void *)pthread, r, strerror(r));
    if (retval != nullptr)
        *retval = t.retval;
    t.retval = nullptr;
    libc_pthread_mutex_unlock(&global_mutex);

    // log
    if (r == 0)
        logTraceForkJoin("join", &pthread);

    return r;
}

void pthread_exit(void *retval)
{
    initialize();
    libc_pthread_mutex_lock(&global_mutex);
    *pthread_create_shm = 1;
    t.retval = retval;
    debug("pthread_exit(%p)", retval);
    libc_pthread_mutex_unlock(&global_mutex);
    libc_pthread_exit(retval);
    abort();
}

inline bool check_atomic() {
        char *atomic = getenv("ATOMIC");
        if (atomic != nullptr)
        {
            if (atoi(atomic) == 1)
            {
                // fprintf(stderr, "libsched env was 1!\n");
                libc_pthread_mutex_unlock(&global_mutex);
                return true;
            }
            // fprintf(stderr, "libsched env was %s != 1\n", atomic);
        }
        else return false;
}

void cleanup_state(Obligation* ob, Event e) {
    switch (ob->state) {
        case PosReachR:
            avoiding[ob->rf.r].erase(ob);
            looking_for[ob->rf.w].erase(ob);
            break;
        case PosExctdW:
            looking_for[ob->rf.r].erase(ob);

            if (rf_sched->avoid_uninit_writes) {
                avoiding_mem[(uintptr_t) e.addr].erase(ob);
                relevant_obs[(uintptr_t) e.addr].erase(ob);
            }
            break;
        case PosNoInfo:
            break;
        case NegExctdW:
            avoiding[ob->rf.r].erase(ob);
            looking_for_mem[(uintptr_t) e.addr].erase(ob);
            relevant_obs[(uintptr_t) e.addr].erase(ob);
            break;
        case NegReachW:
            if (rf_sched->avoid_uninit_writes) {
                avoiding[ob->rf.w].erase(ob);
            }
            break;
        case NegOtherW:
            looking_for[ob->rf.r].erase(ob);
            break;
        case NegNoInfo:
            break;
    }
}

void add_lf_av(Obligation* ob, Event e) {
    switch (ob->state) {
        case PosReachR:
	    debug("reached r %lu", e.event_id);
            avoiding[ob->rf.r].insert(ob);
            looking_for[ob->rf.w].insert(ob);
            break;
        case PosExctdW:
	    debug("exectd w %lu", e.event_id);
            looking_for[ob->rf.r].insert(ob);
            if (rf_sched->avoid_uninit_writes) {
                avoiding_mem[(uintptr_t) e.addr].insert(ob);
                relevant_obs[(uintptr_t) e.addr].insert(ob);
            }
            break;
        case PosNoInfo:
	    debug("abandon %lu rf %lu", ob->rf.r, ob->rf.w);
            break;
        case NegExctdW:
	    debug("neg exectd w %lu", e.event_id);
            avoiding[ob->rf.r].insert(ob);
            looking_for_mem[(uintptr_t) e.addr].insert(ob);
            relevant_obs[(uintptr_t) e.addr].insert(ob);
            break;
        case NegReachW:
	    debug("neg reach w %lu", e.event_id);
            if (rf_sched->avoid_uninit_writes) {
		debug("avoiding %lu", e.event_id);
                avoiding[ob->rf.w].insert(ob);
            }
            break;
        case NegOtherW:
	    debug("neg reach other w %lu", e.event_id);
            looking_for[ob->rf.r].insert(ob);
            break;
        case NegNoInfo:
	    debug("neg abandon %lu rf %lu", ob->rf.r, ob->rf.w);
            break;
    }
}

const char* statestr(ObligState s) {
    switch (s) {
        case PosReachR:
	    return "PosReachR";
            break;
        case PosExctdW:
	    return "PosExctdW";
            break;
        case PosNoInfo:
	    return "PosNoInfo";
            break;
        case NegExctdW:
	    return "NegExctdW";
            break;
        case NegReachW:
	    return "NegReachW";
            break;
        case NegOtherW:
	    return "NegOtherW";
            break;
        case NegNoInfo:
	    return "NegNoInfo";
            break;
	default:
	    return "UNKOWN";
	    break;
    }


}

bool update_state(Obligation* ob, Event e, bool will_exec_next) {
    ObligState new_state = ob->state;
    bool ret = false;
    debug("event %u", e.event_id);

    // State Transitions 
    if (!ob->is_negation) {
        if (ob->state != PosExctdW && !will_exec_next && ob->rf.r == (uintptr_t) e.event_id) {
            new_state = PosReachR;
        } else if (will_exec_next && ob->rf.w == (uintptr_t) e.event_id) {
            new_state = PosExctdW;
        } else if (ob->state == PosReachR && will_exec_next 
            && (uintptr_t) e.event_id == ob->rf.r ) {
            new_state = PosNoInfo;
        } else if (ob->state == PosExctdW && will_exec_next 
            && ob->rf.r == (uintptr_t) e.event_id) {
            // Reached Obligation!
    	      debug("Obligation %lu rf %lu satisfied!", ob->rf.r, ob->rf.w);
            if (!NO_RFF && !ALL_RFF) {
                update(hash_combine(ob->rf.r, ob->rf.w));
            }
            ob->count -= 1;
            new_state = PosNoInfo;

            if (ob->count == 0) {
                relevant_obs[ob->rf.r].erase(ob);
                relevant_obs[ob->rf.w].erase(ob);
            }
        } else if (ob->state == PosExctdW && will_exec_next 
            && avoiding_mem[(uintptr_t) e.addr].count(ob) > 0 
            && e.is_write) {
            new_state = PosNoInfo;
        }
    } else {
        if (will_exec_next && (uintptr_t) e.event_id == ob->rf.w) {
            new_state = NegExctdW;
        } else if (!will_exec_next && (uintptr_t) e.event_id == ob->rf.w) {
            new_state = NegReachW;
        } else if (will_exec_next && ob->state == NegExctdW
            && e.is_write && looking_for_mem[(uintptr_t) e.addr].count(ob) > 0) {
            new_state = NegOtherW;
        } else if (ob->state == NegOtherW && will_exec_next 
            && (uintptr_t) e.event_id == ob->rf.r) {
            // Satisfied Negation!
        	  debug("Negation %lu rf %lu satisfied!", ob->rf.r, ob->rf.w);
            ob->count -= 1;
            if (ob->count == 0) {
                new_state = NegNoInfo;
                relevant_obs[ob->rf.r].erase(ob);
                relevant_obs[ob->rf.w].erase(ob);
            } else {
                new_state = NegOtherW;
            }
        } else if (ob->state == NegExctdW && will_exec_next && (uintptr_t) e.event_id == ob->rf.r) {
            // Negation broken!
        	  debug("Negation [%lu rf %lu] broken!", ob->rf.r, ob->rf.w);
            if (!NO_RFF && !ALL_RFF) {
                rem(hash_combine(ob->rf.r, ob->rf.w + 1));
            }
        }
    }
    debug("[%lu rf %lu] state %s --> %s", ob->rf.r, ob->rf.w, statestr(ob->state), statestr(new_state));

    if (new_state != ob->state) {
        cleanup_state(ob, e);
        ob->state = new_state;
        add_lf_av(ob, e);
    }

    if (e.is_write && will_exec_next) {
        ob->last_write = (uintptr_t) e.addr;
    }

    return ret;
}

void update_obs(Event e, bool will_exec_next) {
        auto instr_obs = relevant_obs[(uintptr_t) e.event_id];
        for (auto ob : instr_obs) {
            obs_to_visit.push_back(ob);
        }

        for (auto ob : relevant_obs[(uintptr_t) e.addr]) {
            if (instr_obs.count(ob) == 0) {
                obs_to_visit.push_back(ob);
            }
        }        

        for (auto ob : obs_to_visit) {
            update_state(ob, e, will_exec_next);
        }

        obs_to_visit.clear();
}

void record_rfs_if_needed(Event e) {
        if (RECORD_EXACT_RFS || ALL_RFF) {
            if (e.is_write) {
                all_last_writes[(uintptr_t) e.addr] = (uintptr_t) e.event_id;
                all_last_write_threads[(uintptr_t) e.addr] = t.tid;
            } else {

                auto mayb_last_w_t = all_last_write_threads.find((uintptr_t) e.addr);
                if (mayb_last_w_t != all_last_write_threads.end()) {
                    if (mayb_last_w_t->second != t.tid) {
                        auto last_w = all_last_writes[(uintptr_t) e.addr];
                        if (ALL_RFF) {
                            update(hash_combine((uintptr_t) e.event_id, last_w));
                        }

                        if (RECORD_EXACT_RFS) {
                            bool was_ins = all_rfs.insert(std::make_pair(
                                (uintptr_t) e.event_id,
                                last_w
                            )).second;
                            if (was_ins) {
                                fprintf(exact_rfs_file, "%lu,%lu\n", (uintptr_t) e.event_id, last_w);
                                fflush(exact_rfs_file);
                            }
                        }
                    }
                }
            }
            
        }
}


extern "C"
{

    uint64_t thread_switch(const void *instr_addr, const void *mem_addr, size_t size, bool is_write, const void *func, EventId *pending_obligation, bool is_negation)
    {
        initialize();

        libc_pthread_mutex_lock(&global_mutex);
        // @TODO move context hash to sched.c -- instr addr will just be that hash
        // int h = get_context_hash();
       
        Event e;
        e.event_id = instr_addr;
        e.addr = mem_addr;
        e.func = (void*) func;
        e.size = size;
        e.is_write = is_write;
        t.next_event = std::optional<Event>{e};

        EventId current = (EventId) e.event_id;
        EventId current_mem = (uintptr_t) mem_addr;

        if (!POS_ONLY) {
            update_obs(e, false);
        }

        signal();
        wait(nullptr);
        t.next_event = std::nullopt;
        // resuming execution -- no more switches!

        if (!POS_ONLY) {
            update_obs(e, true);
        }

        record_rfs_if_needed(e);

        libc_pthread_mutex_unlock(&global_mutex);
        // }
        return 0;
    }

} // extern "C"

void *mallocX(size_t size)
{
    initialize();
    int result = libc_pthread_mutex_trylock(&global_mutex);

    if (result == 0) {
        // Acquired
        // No need to switch on malloc, only free

        void* ptr = libc_malloc(size);
        logTrace("malloc", ptr, -1, false, size);

        libc_pthread_mutex_unlock(&global_mutex);
        return ptr;
     
    }     
    return libc_malloc(size);
    
}

void freeX(void *ptr)
{
    initialize();
    int result = libc_pthread_mutex_trylock(&global_mutex);

    if (result == 0) {
        // Acquired

    // TODO Ok for now, but any other preloaded functions that might be
    // called by pthread need to check the thread is alive too
        if (t.alive){
            signal();
            wait(nullptr);
            logTrace("free", ptr);
        }
        libc_pthread_mutex_unlock(&global_mutex);
            }     
    libc_free(ptr);
}

// logging function
void logTrace(std::string type, void *ptr, int thId, bool flag, size_t size, uintptr_t hash)
{
    if (to_log != nullptr && log_dir != nullptr && strcmp(to_log, "true") == 0)
    {
        FILE *file = nullptr;
        file = fopen(log_dir, "a+");
        if (file != nullptr)
        {
            if (type.compare("rel") == 0)
                fprintf(file, "%d|rel(%p)|%lu\n", (!flag ? gettid() : thId), ptr, hash);
            else if (type.compare("acq") == 0)
                fprintf(file, "%d|acq(%p)|%lu\n", (!flag ? gettid() : thId), ptr, hash);
            else if (type.compare("wait_sleep") == 0)
                fprintf(file, "%d|wait_sleep(%p)|%d\n", (!flag ? gettid() : thId), ptr, 0);
            else if (type.compare("wait_wake") == 0)
                fprintf(file, "%d|wait_wake(%p,%d)|%d\n", (!flag ? gettid() : thId), ptr, gettid(), 0);
            else if (type.compare("signal") == 0)
                fprintf(file, "%d|signal(%p, %d)|%d\n", (!flag ? gettid() : thId), ptr, thId, 0);
            else if (type.compare("sigAll") == 0)
                fprintf(file, "%d|sigAll(%p)|%d\n", (!flag ? gettid() : thId), ptr, 0);
            else if (type.compare("free") == 0)
                fprintf(file, "%d|free(%lu)|%d\n", (!flag ? gettid() : thId), (uintptr_t) ptr, 0);
            else if (type.compare("malloc") == 0)
                fprintf(file, "%d|malloc(%lu, %lu)|%d\n", (!flag ? gettid() : thId), (uintptr_t) ptr, size, 0);
            else
                warning("no suitable logging type!");
            fclose(file);
        }
        else
        {
            warning("file = nullptr, please check whether the file/dir has been created");
        }
    }
}

void logTraceForkJoin(std::string type, pthread_t *ptr)
{
    if (to_log != nullptr && log_dir != nullptr && strcmp(to_log, "true") == 0)
    {
        FILE *file = nullptr;
        file = fopen(log_dir, "a+");
        if (file != nullptr)
        {
            if (type.compare("fork") == 0)
            {
                fprintf(file, "%d|fork(%lu)|%d\n", gettid(), *ptr, 0);
            }
            else if (type.compare("join") == 0)
            {
                fprintf(file, "%d|join(%lu)|%d\n", gettid(), *ptr, 0);
            }
            else
                warning("no suitable logging type!");
            fclose(file);
        }
        else
        {
            warning("file = nullptr, please check whether the file/dir has been created");
        }
    }
}

void logThreadMapping()
{
    if (to_log != nullptr && log_dir != nullptr && strcmp(to_log, "true") == 0)
    {
        FILE *file = nullptr;
        file = fopen(log_dir, "a+");
        if (file != nullptr)
        {
            fprintf(file, "pthread(%lu) = tid(%d)\n", pthread_self(), gettid());
            fclose(file);
        }
        else
        {
            warning("file = nullptr, please check whether the file/dir has been created");
        }
    }
}

extern "C"
{
    Sched *get_sched()
    {
        initialize();
        return rf_sched;
    }
}



//
// Additional Wrappers
//


int sigwait(const sigset_t *ss, int *restrict) {
    libc_pthread_mutex_lock(&global_mutex);
    suspend(nullptr, "sigwait");
    libc_pthread_mutex_unlock(&global_mutex);

    return 0;
}

