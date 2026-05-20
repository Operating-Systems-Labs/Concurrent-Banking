/* Timer thread implementation, clock ticks, and timeout/event signaling. */
#define _DEFAULT_SOURCE  /* Required for usleep with -std=c11 */
#include "timer.h"
#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// Global simulation clock (shared by all threads)
volatile int global_tick;
pthread_mutex_t tick_lock;
pthread_cond_t tick_changed;

// volatile for visibility across all threads
static bool all_transactions_done = false; 

// Timer thread increments clock every TICK_INTERVAL_MS
// Check -> Sleep -> Increment
void* timer_thread(void* arg) {
    // while (!all_transactions_done) {

    // Use while true instead to immediately lock the mutex: The thread cannot know when to stop just with its own cpu cache so it has to
    // enter the loop to check the timer thread for the official update

    if (verbose_logging) {
        printf("Timer thread started (tick interval: %dms)\n", *(int*)arg);
    }

    while (true) {
    // while (global_tick < 10) { //for testing, run the timer for 100 ticks then stop. Change back to while loop with all_transactions_done flag for final version.
        // printf("Global Tick: %d\n", global_tick); //for debugging, can remove later

        // Protect the read of all_transactions_done with the mutex
        // Upon lock, timer_thread (or manager) checks if time is done
        pthread_mutex_lock(&tick_lock);
        if (all_transactions_done) {
            // If done, let go of the lock and leave for other threads to read the lock
            pthread_mutex_unlock(&tick_lock);
            break;
        }
        // If not time yet, let go of the lock so other threads can check the clock while timer_thread takes a nap 
        pthread_mutex_unlock(&tick_lock);
        
        // Speed of the simulation - Simulated clock tick delay
        usleep(*(int*)arg * 1000); //tick_interval_ms = 100ms for now to increment clock again

        // timer_thread wakes up and holds the lock to update the timme
        pthread_mutex_lock(&tick_lock);
        global_tick++;
        // sleep(*(int*)arg); //for testing, tick every 1 second to make it easier to observe the transactions executing at their respective ticks. Change back to usleep with arg for final version.
        
        // Wake all transactions waiting for this new tick
        pthread_cond_broadcast(&tick_changed);
        // Let go of the lock so threads can see the new time
        pthread_mutex_unlock(&tick_lock);
    }
    return NULL;
}

void wait_until_tick(int target_tick) {
    pthread_mutex_lock(&tick_lock);
    while (global_tick < target_tick) {
        pthread_cond_wait(&tick_changed, &tick_lock);
    }
    pthread_mutex_unlock(&tick_lock);
}

void timer_stop(void) {
    // Protect the write with the mutex to avoid data race with timer_thread
    // Make sure timer_thread isnt in the middle of changing the clock at the same moment
    pthread_mutex_lock(&tick_lock);
    all_transactions_done = true;
    pthread_cond_broadcast(&tick_changed); // Help wake every thread waiting on the clock
    pthread_mutex_unlock(&tick_lock);
}

void timer_init(void) {
    global_tick = 0;
    all_transactions_done = false;
    pthread_mutex_init(&tick_lock, NULL);
    pthread_cond_init(&tick_changed, NULL);
}

void timer_destroy(void) {
    pthread_mutex_destroy(&tick_lock);
    pthread_cond_destroy(&tick_changed);
}