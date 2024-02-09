typedef unsigned __int128 uint128_t;

static uint8_t INV_MAX_LOAD_FACTOR = 2;

/*
 * Greg's patented hash function:
 */
static uint64_t hash64to64(uintptr_t x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9U;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebU;
    x ^= x >> 31;
    return x;
}

typedef struct Map
{
    uint32_t capacity;
    uint32_t count;
    uint32_t internal_count;
    uint8_t *buffer;
    uint32_t entry_size;
} Map;

Map *create(uint32_t entry_size, uint32_t capacity)
{
    Map *m = malloc(sizeof(Map));
    m->capacity = capacity;
    m->count = 0;
    m->internal_count = 0;
    m->entry_size = entry_size;
    m->buffer = calloc(capacity, entry_size);
    return m;
}

typedef struct
{
    uintptr_t addrs;
    pid_t tid;
} Vtype;

typedef uintptr_t Ktype;

typedef struct
{
    Ktype key;
    Vtype val;
} Entry;

static inline Vtype special_deleted_val()
{
    Vtype v = {.addrs = UINT64_MAX, .tid = 0};
    return v;
}

static inline Vtype special_null_val()
{
    Vtype v = {.addrs = 0, .tid = 0};
    return v;
}

static inline bool is_deleted(Vtype v)
{
    return v.addrs == UINT64_MAX;
}

void insert(Map *m, Ktype k, Vtype v)
{
    if (m->capacity < m->internal_count * INV_MAX_LOAD_FACTOR)
    {
        Map *new_m = create(sizeof(Entry), m->capacity * 2);
        Entry *buffer = (Entry *)m->buffer;
        for (int i = 0; i < m->capacity; i += 1)
        {
            if (buffer[i].key != 0 && !is_deleted(buffer[i].val))
            {
                insert(new_m, buffer[i].key, buffer[i].val);
            }
        }

        free(m->buffer);
        m->buffer = new_m->buffer;
        m->capacity = new_m->capacity;
        fprintf(stderr, "\t realloced hash map to %d\n", m->capacity * 2);
    }

    uint32_t offset = (uint32_t)hash64to64(k) % m->capacity;
    Entry *buffer = (Entry *)m->buffer;
    Entry *entry = &buffer[offset];

    // linear probing
    while (entry->key != 0 && entry->key != k)
    {
        // fprintf(stderr, "probing insert\n");
        offset = (offset + 1) % m->capacity;
        entry = &buffer[offset];
    }

    entry->key = k;
    entry->val = v;
    // fprintf(stderr, "insert %lu:%lu@%lu\n", k, v, entry);
    m->count += 1;
    m->internal_count += 1;
}

Vtype get(Map *m, Ktype k)
{
    uint32_t offset = (uint32_t)hash64to64(k) % m->capacity;
    uint32_t attempts = 0;
    Entry *buffer = (Entry *)m->buffer;
    Entry *entry = &buffer[offset];


    while (entry->key != 0 && entry->key != k )
    {
        // fprintf(stderr, "probing read \n");
        offset = (offset + 1) % m->capacity;
        entry = &buffer[offset];
    }


    if (is_deleted(entry->val))
    {
        return special_null_val();
    }
    // fprintf(stderr, "get %lu from %llu@%lu\n", entry->val, entry->key, entry);
    return entry->val;
}

void erase(Map *m, Ktype k)
{
    uint32_t offset = (uint32_t)hash64to64(k) % m->capacity;
    Entry *buffer = (Entry *)m->buffer;
    Entry *entry = &buffer[offset];

    while (entry->key != 0 && entry->key != k)
    {
        // fprintf(stderr, "probing read \n");
        offset = (offset + 1) % m->capacity;
        entry = &buffer[offset];
    }

    if (entry->key) {
        m->count -= 1;
        // fprintf(stderr, "get %lu from %llu@%lu\n", entry->val, entry->key, entry);
        entry->val = special_deleted_val();
    }
}

