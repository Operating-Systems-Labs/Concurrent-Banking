/* Bounded buffer implementation using synchronization primitives. */
#include "buffer_pool.h"
#include <stdio.h>
#include <time.h>

void init_buffer_pool(BufferPool* pool) {
    // Make all spots on the table as empty
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        pool->slots[i].in_use = false;
        pool->slots[i].account_id = -1;
        pool->slots[i].data = NULL;
    }
    // Fill empty_slots bowl with tickets
    sem_init(&pool->empty_slots, 0, BUFFER_POOL_SIZE);
    // Empty full_slots bowl
    sem_init(&pool->full_slots, 0, 0);             
    // Ensure only one transaction worker can look at the table at a time to see which spots are empty or full
    pthread_mutex_init(&pool->pool_lock, NULL);     
    pool->total_loads = 0;
    pool->total_unloads = 0;
    pool->current_in_use = 0;
    pool->peak_in_use = 0;
    pool->blocked_loads = 0;
}

void destroy_buffer_pool(BufferPool* pool) {
    sem_destroy(&pool->empty_slots);
    sem_destroy(&pool->full_slots);
    pthread_mutex_destroy(&pool->pool_lock);
}

// When a transaction worker needs an account, take a ticket from the empty_slots bowl
void load_account(BufferPool* pool, int account_id) {
    if (sem_trywait(&pool->empty_slots) != 0) {
        pthread_mutex_lock(&pool->pool_lock);
        pool->blocked_loads++;
        pthread_mutex_unlock(&pool->pool_lock);
        sem_wait(&pool->empty_slots); // Block if pool is full
    }

    // Once there is space, grab pool lock so no one else can move folders around
    pthread_mutex_lock(&pool->pool_lock);
    // Find a free slot and "load" the account
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!pool->slots[i].in_use) {
            pool->slots[i].account_id = account_id;
            pool->slots[i].in_use = true;
            pool->total_loads++;
            pool->current_in_use++;
            if (pool->current_in_use > pool->peak_in_use) {
                pool->peak_in_use = pool->current_in_use;
            }
            break;
        }
    }

    pthread_mutex_unlock(&pool->pool_lock);
    // Put ticket into the full_slots bowl to let everyone know there is a new acc to work on 
    sem_post(&pool->full_slots);

    if (BUFFER_POOL_HOLD_US > 0) {
        struct timespec hold = {
            .tv_sec = 0,
            .tv_nsec = BUFFER_POOL_HOLD_US * 1000L
        };
        nanosleep(&hold, NULL);
    }
}


// Clean up to put account back to the filing cabinet to make room for others
void unload_account(BufferPool* pool, int account_id) {
    // Take a ticket from full_slots bowl
    sem_wait(&pool->full_slots);
    // Lock the table
    pthread_mutex_lock(&pool->pool_lock);

    // Clear that spot on the table as empty
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->slots[i].in_use && pool->slots[i].account_id == account_id) {
            pool->slots[i].in_use = false;
            pool->slots[i].account_id = -1;
            pool->total_unloads++;
            if (pool->current_in_use > 0) {
                pool->current_in_use--;
            }
            break;
        }
    }

    pthread_mutex_unlock(&pool->pool_lock);
    // Put ticket back into empty_slots blow and let next transaction worker know there is a free spot available
    sem_post(&pool->empty_slots);
}

void print_buffer_pool_report(const BufferPool* pool) {
    printf("\n=== Buffer Pool Report ===\n");
    printf("Pool size: %d slots\n", BUFFER_POOL_SIZE);
    printf("Total loads: %d\n", pool->total_loads);
    printf("Total unloads: %d\n", pool->total_unloads);
    printf("Peak usage: %d slots\n", pool->peak_in_use);
    printf("Blocked operations (pool full): %d\n", pool->blocked_loads);
}