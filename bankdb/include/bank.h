/* Declarations for bank and account data structures and related interfaces. */
#ifndef BANK_H
#define BANK_H
#define MAX_ACCOUNTS 100

#include <stdbool.h>
#include <pthread.h>

typedef struct {
    int account_id;         
    int balance_centavos;   
    pthread_rwlock_t lock;  
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int num_accounts;
    pthread_mutex_t bank_lock;  // Protects bank metadata
} Bank;

extern Bank* bank; // Global bank instance for now, remove when buffer pool is implemented  

Bank* create_bank();

void destroy_bank(Bank* bank);

// NOTE: account creation is handled by load_accounts_file; no separate API.

#endif 