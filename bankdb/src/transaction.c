/* Transaction worker execution logic, scheduling, and operation dispatch when user wants to move mo. */
#include "bank.h" // Include bank.h to access the global bank instance and account operations. Remove when buffer pool is implemented.
#include "transaction.h"
#include "timer.h"
#include "metrics.h"
#include "buffer_pool.h"
#include "lock_mgr.h"
#include <stdio.h>
#include <stdbool.h>

extern BufferPool buffer_pool;
extern bool verbose_logging;

static int current_tick(void) {
    int tick;
    pthread_mutex_lock(&tick_lock);
    tick = global_tick;
    pthread_mutex_unlock(&tick_lock);
    return tick;
}

static const char* op_type_name(OpType type) {
    switch (type) {
        case OP_DEPOSIT: return "DEPOSIT";
        case OP_WITHDRAW: return "WITHDRAW";
        case OP_TRANSFER: return "TRANSFER";
        case OP_BALANCE: return "BALANCE";
        default: return "UNKNOWN";
    }
}

// Helper to find account with matching id and lock it appropriately
static Account* find_account(int account_id) {
    for(int i = 0; i < bank->num_accounts; i++) {
        if (bank->accounts[i].account_id == account_id) {
            return &bank->accounts[i];
        }
    }
    return NULL;
}

// static void print_loaded_accounts(Bank* bank) {
//     printf("Loaded %d accounts:\n", bank->num_accounts);
//     for (int i = 0; i < bank->num_accounts; i++) {
//         printf("Account ID: %d, Balance: %d centavos\n", bank->accounts[i].account_id, bank->accounts[i].balance_centavos);
//     }
// }

// Handle logic race between get_balance and transfer
// Sol: Wrap every account access in a read/write lock

// rdlock before reading balance and unlock after - multiple workers can check the account balance at the same time without changing anything
static int get_balance(int account_id) {
    if (verbose_logging) {
        printf("Tick %d: BALANCE account %d\n", current_tick(), account_id);
    }
    load_account(&buffer_pool, account_id);
    Account* acc = find_account(account_id);
    if (!acc) {
        unload_account(&buffer_pool, account_id);
        return 0;
    }

    lock_account(acc, LOCK_READ);
    int balance = acc->balance_centavos;
    unlock_account(acc);
    unload_account(&buffer_pool, account_id);

    return balance;
}

// rwlock so no one else can look at the account until done with the update (deposit and withdraw)
static void deposit(int account_id, int amount_centavos) {
    if (verbose_logging) {
        printf("Tick %d: DEPOSIT account %d amount %d\n", current_tick(), account_id, amount_centavos);
    }
    load_account(&buffer_pool, account_id);
    Account* acc = find_account(account_id);
    if (!acc) {
        unload_account(&buffer_pool, account_id);
        return;
    }

    lock_account(acc, LOCK_WRITE);
    acc->balance_centavos += amount_centavos;
    unlock_account(acc);

    unload_account(&buffer_pool, account_id);

    record_deposit(amount_centavos);
}

static bool withdraw(int account_id, int amount_centavos) {
    if (verbose_logging) {
        printf("Tick %d: WITHDRAW account %d amount %d\n", current_tick(), account_id, amount_centavos);
    }
    load_account(&buffer_pool, account_id);
    Account* acc = find_account(account_id);
    if (!acc) {
        unload_account(&buffer_pool, account_id);
        return false;
    }

    lock_account(acc, LOCK_WRITE);
    if (acc->balance_centavos >= amount_centavos) {
        acc->balance_centavos -= amount_centavos;
        unlock_account(acc);

        unload_account(&buffer_pool, account_id);

        record_withdraw(amount_centavos);
        return true;
    } 
    
    unlock_account(acc);
    unload_account(&buffer_pool, account_id);
    if (verbose_logging) {
        printf("Tick %d: WITHDRAW failed (insufficient funds) account %d\n", current_tick(), account_id);
    }
    return false; // Account not found, treat as insufficient funds for simplicity for now
}

// use rwlock wrlock for both source and destination accounts before changing balances
static bool transfer(int from_id, int to_id, int amount_centavos) {
    int from_index = -1;
    int to_index = -1;
    int i;
    bool success = false;
    Account *acc_from, *acc_to;

    if (from_id == to_id || amount_centavos <= 0) return false;

    if (verbose_logging) {
        printf("Tick %d: TRANSFER from %d to %d amount %d\n", current_tick(), from_id, to_id, amount_centavos);
    }

    // Locate account indices
    for (i = 0; i < bank->num_accounts; i++) {
        if (bank->accounts[i].account_id == from_id) from_index = i;
        if (bank->accounts[i].account_id == to_id) to_index = i;
    }

    if (from_index < 0 || to_index < 0) return false;

    acc_from = &bank->accounts[from_index];
    acc_to = &bank->accounts[to_index];

    load_account(&buffer_pool, from_id);
    load_account(&buffer_pool, to_id);

    lock_accounts_ordered(acc_from, from_id, acc_to, to_id);

    if (bank->accounts[from_index].balance_centavos >= amount_centavos) {
        bank->accounts[from_index].balance_centavos -= amount_centavos;
        bank->accounts[to_index].balance_centavos += amount_centavos;
        success = true;
    }

    unlock_accounts_ordered(acc_from, from_id, acc_to, to_id);

    unload_account(&buffer_pool, to_id);
    unload_account(&buffer_pool, from_id);

    if (success) {
        record_withdraw(amount_centavos);
        record_deposit(amount_centavos);
    }

    return success;
}

void* execute_transaction(void* arg) {

    Transaction* tx = (Transaction*)arg;

    // Wait for scheduled start by the timer
    wait_until_tick(tx->start_tick);

    // Capture metrics
    pthread_mutex_lock(&tick_lock);
    tx->actual_start = global_tick;
    pthread_mutex_unlock(&tick_lock);

    if (verbose_logging) {
        printf("Tick %d: T%d started\n", tx->actual_start, tx->tx_id);
    }

    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];

        int target_tick = op->tick;
        wait_until_tick(target_tick);

        // // Placeholder for now: just print progress
        // printf("T%d reached op %d at tick %d\n", tx->tx_id, i, target_tick);
        // print_operation(op);

        switch (op->type) {
            case OP_DEPOSIT:
                deposit(op->account_id, op->amount_centavos);
                break;
                
            case OP_WITHDRAW:
                if (!withdraw(op->account_id, op->amount_centavos)) {
                    // Insufficient funds - abort transaction
                    tx->status = TX_ABORTED;
                    break;
                }
                break;
                
            case OP_TRANSFER:
                if (!transfer(op->account_id, op->target_account,
                              op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    break;
                }
                break;
                
            case OP_BALANCE:
                int balance = get_balance(op->account_id);
                printf("T%d: Account %d balance = PHP %d.%02d\n", 
                       tx->tx_id, op->account_id, 
                       balance / 100, balance % 100);
                break;
        }

        if (verbose_logging) {
            printf("Tick %d: T%d completed %s\n", current_tick(), tx->tx_id, op_type_name(op->type));
        }

        if (tx->status == TX_ABORTED) {
            break;
        }

    }

    pthread_mutex_lock(&tick_lock);
    tx->actual_end = global_tick;
    pthread_mutex_unlock(&tick_lock);

    if (tx->status != TX_ABORTED) {
        tx->status = TX_COMMITTED;
        if (verbose_logging) {
            printf("Tick %d: T%d committed\n", current_tick(), tx->tx_id);
        }
    } else if (verbose_logging) {
        printf("Tick %d: T%d aborted\n", current_tick(), tx->tx_id);
    }
    return NULL;
}