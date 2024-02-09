typedef unsigned __int128 uint128_t;

static uint8_t INV_MAX_LOAD_FACTOR = 2;

/*
 * Greg's patented hash function:
 */
static uint64_t hash64to64(uint64_t x)
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
    Map *m = (Map*) malloc(sizeof(Map));
    m->capacity = capacity;
    m->count = 0;
    m->internal_count = 0;
    m->entry_size = entry_size;
    m->buffer = (uint8_t*) calloc(capacity, entry_size);
    return m;
}

Map *coopt(uint32_t entry_size, uint8_t* mem, size_t size_bytes)
{
    Map *m = (Map*) malloc(sizeof(Map));
    m->capacity = size_bytes / entry_size;
    m->count = 0;
    m->internal_count = 0;
    m->entry_size = entry_size;
    m->buffer = mem;
    return m;
}

typedef float Vtype;

typedef uint64_t Ktype;

typedef struct
{
    Ktype key;
    Vtype val;
} Entry;

static inline Vtype special_deleted_val()
{
    Vtype v = -0.0;
    return v;
}

static inline Vtype special_null_val()
{
    Vtype v = 0.0;
    return v;
}

static inline bool is_deleted(Vtype v)
{
    return v == special_deleted_val();
}

void insert(Map *m, Ktype k, Vtype v)
{
    if (m->capacity < m->internal_count * INV_MAX_LOAD_FACTOR)
    {
        fprintf(stderr, "\t OUT OF SPACE\n", m->capacity * 2);
        abort();
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
    if (entry->val == special_null_val()) {
        m->internal_count += 1;
    }
    m->count += 1;
    entry->val = v;
    // fprintf(stderr, "insert %lu:%lu@%lu\n", k, v, entry);
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

