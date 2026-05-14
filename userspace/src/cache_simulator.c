#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "cache_simulator.h"

#define READ_BATCH_SIZE 256
#define INITIAL_TRACE_CAPACITY 1024

static uint32_t global_timer = 0;

static uint32_t num_lvls;
static uint32_t line_size;
static uint32_t assoc[3];
static uint32_t lvl_size[3];
static ReplacementPolicy policy;

// Efficient calculation of fast_log2 for numbers that are powers of two
static inline uint32_t fast_log2(uint32_t n) {
    uint32_t log = 0;
    while (n >>= 1) ++log;
    return log;
}

static bool is_power_of_two(uint32_t x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

static int cache_init(CacheLevel *cache, uint8_t lvl) {
    cache->size = lvl_size[lvl - 1];
    cache->associativity = assoc[lvl - 1];
    cache->line_size = line_size;
    cache->num_sets = lvl_size[lvl - 1] / (line_size * assoc[lvl - 1]);
    cache->hits = 0;
    cache->misses = 0;

    if (!is_power_of_two(cache->num_sets)) {
        fprintf(stderr, "Greska: Broj setova (%u) nije stepen broja 2!\n", cache->num_sets);
        return -1;
    }

    cache->sets = malloc(cache->num_sets * sizeof(CacheSet));
    for (uint32_t i = 0; i < cache->num_sets; i++) {
        cache->sets[i].lines = calloc(assoc[lvl - 1], sizeof(CacheLine));
    }
    return 0;
}

static int find_optimal_victim(CacheLevel *cache, CacheSet *set, mem_access *traces, size_t current_idx, size_t total_traces) {
    int victim_idx = 0;
    size_t furthest_appearance = 0;

    uint32_t offset_bits = (uint32_t)fast_log2(cache->line_size);
    uint32_t index_bits = (uint32_t)fast_log2(cache->num_sets);

    for (uint32_t i = 0; i < cache->associativity; i++) {
        size_t next_use = SIZE_MAX; // Assume it is no longer used
        
        // Search for the next use of this tag in the future
        for (size_t j = current_idx + 1; j < total_traces; j++) {
            uint64_t future_tag = traces[j].addr >> (offset_bits + index_bits);
            if (future_tag == set->lines[i].tag) {
                next_use = j;
                break;
            }
        }

        if (next_use == SIZE_MAX) return i; // If it is never used again, it is a perfect victim

        if (next_use > furthest_appearance) {
            furthest_appearance = next_use;
            victim_idx = i;
        }
    }
    return victim_idx;
}

static void cache_access(CacheLevel *cache, mem_access *all_traces, size_t current_idx, size_t total_traces) {
    global_timer++;
    uint64_t address = all_traces[current_idx].addr;

    uint32_t offset_bits = (uint32_t)fast_log2(cache->line_size);
    uint32_t index_bits = (uint32_t)fast_log2(cache->num_sets);
    
    uint64_t index = (address >> offset_bits) & (cache->num_sets - 1);
    uint64_t tag = address >> (offset_bits + index_bits);

    CacheSet *set = &cache->sets[index];

    // 1. Hit check
    for (uint32_t i = 0; i < cache->associativity; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            cache->hits++;
            set->lines[i].last_used = global_timer;
            return;
        }
    }

    // 2. MISS - look for an empty slot
    cache->misses++;
    for (uint32_t i = 0; i < cache->associativity; i++) {
        if (!set->lines[i].valid) {
            set->lines[i].valid = true;
            set->lines[i].tag = tag;
            set->lines[i].last_used = global_timer;
            return;
        }
    }

    // 3. Eviction (If the set is full)
    int victim_idx = 0;
    if (policy == ALG_LRU) {
        uint32_t min_time = UINT32_MAX;
        for (uint32_t i = 0; i < cache->associativity; i++) {
            if (set->lines[i].last_used < min_time) {
                min_time = set->lines[i].last_used;
                victim_idx = i;
            }
        }
    } else {
        victim_idx = find_optimal_victim(cache, set, all_traces, current_idx, total_traces);
    }

    set->lines[victim_idx].tag = tag;
    set->lines[victim_idx].last_used = global_timer;
}

static void cache_free(CacheLevel *cache) {
    for (uint32_t i = 0; i < cache->num_sets; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
}

static mem_access* load_traces(const char *device_path, size_t *out_count) {
    size_t capacity = INITIAL_TRACE_CAPACITY;
    size_t count = 0;
    
    mem_access *traces = malloc(capacity * sizeof(mem_access));
    if (!traces) {
        perror("Greska: Nije moguce alocirati pocetnu memoriju za tragove");
        *out_count = 0;
        return NULL;
    }

    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Greska: Ne mogu otvoriti /dev/trace_collector");
        free(traces);
        *out_count = 0;
        return NULL;
    }

    mem_access buffer[READ_BATCH_SIZE];
    ssize_t bytes_read;

    // Read from the kernel driver as long as there is data
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        size_t entries = bytes_read / sizeof(mem_access);
        
        // Check if the array needs to be expanded
        if (count + entries > capacity) {
            capacity *= 2; // Double the capacity
            mem_access *temp = realloc(traces, capacity * sizeof(mem_access));
            if (!temp) {
                perror("Greska: Nije moguce realocirati memoriju za tragove");
                close(fd);
                *out_count = count;
                return traces; // Return what has been loaded so far
            }
            traces = temp;
        }
        
        // Copy the read data into the main array
        memcpy(&traces[count], buffer, entries * sizeof(mem_access));
        count += entries;
    }

    if (bytes_read < 0) {
        perror("Greska pri citanju iz /dev/trace_collector");
    }

    close(fd);
    *out_count = count;
    
    // Optional: shrink the allocated memory to the exact size (shrink to fit)
    if (count > 0 && count < capacity) {
        mem_access *temp = realloc(traces, count * sizeof(mem_access));
        if (temp) {
            traces = temp;
        }
    }

    return traces;
}

static void get_valid_input(uint32_t *target, const char *prompt, const char *invalid_num_msg, bool (*isValid)(uint32_t, uint8_t), uint8_t lvl, uint8_t step) {
    int status;

    while (1) {
        if (lvl == 0)
        {
            printf("%" PRIu8 ")%s", step, prompt);
        }
        else
        {
            printf("%" PRIu8 ".%" PRIu8 ")%s",step, lvl, prompt);
        }
        status = scanf("%"SCNu32 , target);

        if (status != 1) {
            printf("Invalid input! Please enter a number.\n");
            while (getchar() != '\n'); // Clear buffer
        } 
        else if (!isValid(*target, lvl)) {
            if (step != 4)
            {
                printf("%s\n", invalid_num_msg);
            }
            else if (lvl == 1)
            {
                printf("%s%d i 2048\n", invalid_num_msg, assoc[0] * line_size);
            }
            else if (lvl == 2)
            {
                printf("%s%d i 4096\n", invalid_num_msg, assoc[1] * line_size);
            }
            else if (lvl == 3)
            {
                printf("%s%d i 8192\n", invalid_num_msg, assoc[2] * line_size);
            }
        } 
        else {
            // Success
            break;
        }
    }
}

static bool is_valid_num_lvls(uint32_t num_lvls, uint8_t ignore)
{
    return num_lvls >= 1 && num_lvls <=3;
}
static bool is_valid_line_size(uint32_t line_size, uint8_t ignore)
{
    return line_size == 16 || line_size == 32 || line_size == 64;
}
static bool is_valid_assoc(uint32_t assoc, uint8_t ignore)
{
    return is_power_of_two(assoc) && assoc <=8;
}
static bool is_valid_lvl_size(uint32_t lvl_size, uint8_t lvl)
{
    uint32_t lvl_max_size;
    if (lvl == 1)
    {
        lvl_max_size = 2048;
    }
    else if (lvl == 2)
    {
        lvl_max_size = 4096;
    }
    else if (lvl == 3)
    {
        lvl_max_size = 8192;
    }
    return is_power_of_two(lvl_size) && lvl_size >= assoc[lvl - 1] * line_size && lvl_size <= lvl_max_size;
}
static bool is_valid_algorithm_num(uint32_t algorithm_num, uint8_t ignore)
{
    return algorithm_num == 1 || algorithm_num == 2;
}

static void configure_cache_sim_parameters()
{
    printf("\nSimulator keš memorije\n============================================================================\n\n");

    get_valid_input(&num_lvls, " Unesite broj nivoa keš memorije (1-3): ", "Broj nivoa keša mora da bude u opsegu 1-3!", is_valid_num_lvls, 0, 1);
    get_valid_input(&line_size, " Unesite veličinu keš linije: ", "Veličina keš linije mora biti 16, 32 ili 64!", is_valid_line_size, 0, 2);
    get_valid_input(&assoc[0], " Unesite asocijativnost prvog nivoa keš memorije (stepen broja 2, max 8): ", "Asocijativnost mora biti 1, 2, 4 ili 8!", is_valid_assoc, 1, 3);
    if (num_lvls > 1)
    {
        get_valid_input(&assoc[1], " Unesite asocijativnost drugog nivoa keš memorije (stepen broja 2, max 8): ", "Asocijativnost mora biti 1, 2, 4 ili 8!", is_valid_assoc, 2, 3);
    }
    if (num_lvls > 2)
    {
        get_valid_input(&assoc[2], " Unesite asocijativnost treceg nivoa keš memorije (stepen broja 2, max 8): ", "Asocijativnost mora biti 1, 2, 4 ili 8!", is_valid_assoc, 3, 3);
    }
    get_valid_input(&lvl_size[0], " Unesite veličinu prvog nivoa keš memorije: ", "Veličina ovog nivoa mora biti stepen broja 2 između ", is_valid_lvl_size, 1, 4);
    if (num_lvls > 1)
    {
        get_valid_input(&lvl_size[1], " Unesite veličinu drugog nivoa keš memorije: ", "Veličina ovog nivoa mora biti stepen broja 2 između ", is_valid_lvl_size, 2, 4);
    }
    if (num_lvls > 2)
    {
        get_valid_input(&lvl_size[2], " Unesite veličinu treceg nivoa keš memorije: ", "Veličina ovog nivoa mora biti stepen broja 2 između ", is_valid_lvl_size, 3, 4);
    }

    uint32_t algorithm_num;
    get_valid_input(&algorithm_num, " Odaberite algoritam zamjene (unesite 1 za optimalni ili 2 za LRU): ", "Unesite 1 za optimalni ili 2 za LRU!", is_valid_algorithm_num, 0, 5);

    if (algorithm_num == 1)
    {
        policy = ALG_OPTIMAL;
    }
    else if (algorithm_num == 2)
    {
        policy = ALG_LRU;
    }
}

void run_cache_sim()
{
    // Parameter configuration
    configure_cache_sim_parameters();

    // Loading traces
    size_t total_traces = 0;
    mem_access *traces = load_traces("/dev/trace_collector", &total_traces);
    
    if (!traces || total_traces == 0) {
        printf("Nema učitanih tragova ili je došlo do greške pri čitanju.\n");
        if (traces) free(traces);
        return;
    }

    // Cache initialization
    CacheLevel l1, l2, l3;

    cache_init(&l1, 1);
    if (num_lvls > 1)
    {
        cache_init(&l2, 2);
    }
    if (num_lvls > 2)
    {
        cache_init(&l3, 3);
    }

    // Running the simulation
    for (size_t i = 0; i < total_traces; i++) {
        uint32_t l1_hits_before = l1.hits;
        cache_access(&l1, traces, i, total_traces);

        if (num_lvls > 1 && l1.hits == l1_hits_before) // L1 Miss
        {
            uint32_t l2_hits_before = l2.hits;
            cache_access(&l2, traces, i, total_traces);

            if (num_lvls > 2 && l2.hits == l2_hits_before) // L2 Miss
            {
                cache_access(&l3, traces, i, total_traces);
            }
        }
    }
    
    // Printing statistics
    printf("\n============================================================================\n");
    printf("   IZVJESTAJ HIJERARHIJSKE SIMULACIJE (%" PRIu32 " NIVO%s)\n", num_lvls, num_lvls == 1 ? "" : "A");
    printf("============================================================================\n");
    printf("Ukupno memorijskih pristupa: %zu\n\n", total_traces);
    size_t num_reads = 0;
    for (int i = 0; i < total_traces; i++)
    {
        if (!traces[i].type)
        {
            num_reads++;
        }
    }
    printf("Broj citanja: %zu\n", num_reads);
    printf("Broj upisa: %zu\n", total_traces - num_reads);

    // L1 Statistics
    printf("L1 CACHE:\n");
    printf("  Pogoci:    %" PRIu64 "\n", l1.hits);
    printf("  Promasaji: %" PRIu64 "\n", l1.misses);
    printf("  Miss Rate: %.2f%%\n\n", ((double)l1.misses / (l1.hits + l1.misses)) * 100);

    // L2 Statistics (accesses are actually L1 misses)
    if (num_lvls > 1)
    {
        printf("L2 CACHE:\n");
        printf("  Pogoci:    %" PRIu64 "\n", l2.hits);
        printf("  Promasaji: %" PRIu64 "\n", l2.misses);
        if ((l2.hits + l2.misses) > 0)
        {
            printf("  Lokalni Miss Rate: %.2f%%\n\n", ((double)l2.misses / (l2.hits + l2.misses)) * 100);
        }
    }

    // L3 Statistics (accesses are actually L2 misses)
    if (num_lvls > 2)
    {
        printf("L3 CACHE:\n");
        printf("  Pogoci:    %" PRIu64 "\n", l3.hits);
        printf("  Promasaji: %" PRIu64 "\n", l3.misses);
        if ((l3.hits + l3.misses) > 0)
        {
            printf("  Lokalni Miss Rate: %.2f%%\n\n", ((double)l3.misses / (l3.hits + l3.misses)) * 100);
        }
    }

    // Global statistics (data that passed through all levels and went to RAM)
    uint64_t RAM_visits = (num_lvls == 1) ? l1.misses : (num_lvls == 2) ? l2.misses : l3.misses;
    double global_miss_rate = ((double)RAM_visits / total_traces) * 100;
    printf("GLOBALNA STATISTIKA:\n");
    printf("  Ukupno odlazaka u RAM: %" PRIu64 "\n", RAM_visits);
    printf("  Globalni Miss Rate:    %.2f%%\n", global_miss_rate);
    printf("============================================================================\n");

    // Memory cleanup
    cache_free(&l1);
    if (num_lvls > 1)
    {
        cache_free(&l2);
    }
    if (num_lvls > 2)
    {
        cache_free(&l3);
    }
    free(traces);
}