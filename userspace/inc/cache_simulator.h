#ifndef CACHE_SIM_H
#define CACHE_SIM_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu_uapi.h"

// Tipovi algoritama zamjene
typedef enum {
    ALG_LRU,
    ALG_OPTIMAL
} ReplacementPolicy;

typedef struct {
    uint64_t tag;
    bool valid;
    uint32_t last_used; // Za LRU
} CacheLine;

typedef struct {
    CacheLine *lines;
} CacheSet;

typedef struct {
    uint32_t size;
    uint32_t associativity;
    uint32_t line_size;
    uint32_t num_sets;
    CacheSet *sets;
    
    uint64_t hits;
    uint64_t misses;
} CacheLevel;

void run_cache_sim();

#endif