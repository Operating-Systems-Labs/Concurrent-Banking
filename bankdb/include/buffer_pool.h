/* Declarations for bounded buffer pool structures, synchronization, and APIs. */
#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#define BUFFER_POOL_SIZE 5
#define BUFFER_POOL_HOLD_US 5000

#include <semaphore.h>
#include "bank.h"

typedef struct {
    int account_id;
    Account* data;
    bool in_use;
} BufferSlot;

typedef struct {
    BufferSlot slots[BUFFER_POOL_SIZE];
    sem_t empty_slots;
    sem_t full_slots;
    pthread_mutex_t pool_lock;
    int total_loads;
    int total_unloads;
    int current_in_use;
    int peak_in_use;
    int blocked_loads;
} BufferPool;

void init_buffer_pool(BufferPool* pool);

// Load account into buffer pool (producer)
void load_account(BufferPool* pool, int account_id);

// Unload account from buffer pool (consumer)
void unload_account(BufferPool* pool, int account_id);

void print_buffer_pool_report(const BufferPool* pool);

#endif // BUFFER_POOL_H