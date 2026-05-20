/* Metrics aggregation, timing stats, counters, and reporting output logic. */
#include "bank.h"
#include "transaction.h"
#include "metrics.h"
#include "buffer_pool.h"
#include <stdlib.h>
#include <stdio.h>

long total_deposited = 0;
long total_withdrawn = 0;
pthread_mutex_t metrics_lock;
extern BufferPool buffer_pool;

void metrics_init(void) {
    // Create the lock to protect the record called in main before everything starts
    pthread_mutex_init(&metrics_lock, NULL);
}

void metrics_destroy(void) {
    pthread_mutex_destroy(&metrics_lock);
}

void record_deposit(int amount) {
    pthread_mutex_lock(&metrics_lock);
    total_deposited += amount;
    pthread_mutex_unlock(&metrics_lock);
}

void record_withdraw(int amount) {
    pthread_mutex_lock(&metrics_lock);
    total_withdrawn += amount;
    pthread_mutex_unlock(&metrics_lock);
}

void print_final_report(Bank* bank, Transaction* txs[], int txs_count, long initial_sum) {
    long final_total = 0;

    // 1. Calculate the final balance of the whole bank
    for (int i = 0; i < bank->num_accounts; i++) {
        final_total += (long)bank->accounts[i].balance_centavos;
    }

    // 2. The Conservation Check Math
    long expected_final = (long)initial_sum + total_deposited - total_withdrawn;
    bool check_passed = (final_total == expected_final);

    printf("\n=== Money Conservation Check ===\n");
    printf("Initial total: PHP %ld.%02ld\n", initial_sum / 100, initial_sum % 100);
    
    long net_change = total_deposited - total_withdrawn;
    printf("Net Change:    PHP %+ld.%02ld (In: %ld, Out: %ld)\n", 
           net_change / 100, labs(net_change % 100), total_deposited, total_withdrawn);

    printf("Final total:   PHP %ld.%02ld\n", final_total / 100, labs(final_total % 100));
    printf("Conservation check: %s\n", check_passed ? "PASSED" : "FAILED");

    // 3. Performance Metrics
    printf("\n=== Transaction Performance Metrics ===\n");
    printf("%-5s | %-10s | %-12s | %-8s | %-10s | %-10s\n", 
        "TXID", "Schedule", "ActualStart", "End", "WaitTicks", "Status");
    printf("--------------------------------------------------------------------------\n");

    for (int i = 0; i < txs_count; i++) {
        const char* status = "RUNNING";
        if (txs[i]->status == TX_COMMITTED) {
            status = "COMMITTED";
        } else if (txs[i]->status == TX_ABORTED) {
            status = "ABORTED";
        }
        int wait = txs[i]->actual_start - txs[i]->start_tick;
        printf("T%-4d | %-10d | %-12d | %-8d | %-10d | %-10s\n",
            txs[i]->tx_id, txs[i]->start_tick, txs[i]->actual_start, 
            txs[i]->actual_end, wait, status);
    }

    print_buffer_pool_report(&buffer_pool);
}