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

Account* create_account(int account_id, int balance_centavos);

// void deposit(int account_id, int amount_centavos);

// bool withdraw(int account_id, int amount_centavos);

// bool transfer(int from_id, int to_id, int amount_centavos);

// int get_balance(int account_id);

#endif 