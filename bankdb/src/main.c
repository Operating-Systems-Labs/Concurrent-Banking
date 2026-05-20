/* Entry point: CLI parsing, configuration loading, and subsystem initialization. */

#include "bank.h"
#include "lock_mgr.h"
#include "transaction.h"
#include "timer.h"
#include "utils.h"
#include "metrics.h"
#include "buffer_pool.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

static const char* op_name(OpType t) {
    switch (t) {
        case OP_DEPOSIT: return "DEPOSIT";
        case OP_WITHDRAW: return "WITHDRAW";
        case OP_TRANSFER: return "TRANSFER";
        case OP_BALANCE: return "BALANCE";
        default: return "UNKNOWN";
    }
}

// just for debugging, can remove later
static void print_loaded_transactions(Transaction* txs[], int max_txs) {
    int tx_printed = 0;

    for (int i = 0; i < max_txs; i++) {
        Transaction* tx = txs[i];
        if (tx == NULL) {
            break;
        }

        tx_printed++;
        printf("\nT%d  start_tick=%d  num_ops=%d\n",
            tx->tx_id, tx->start_tick, tx->num_ops);

        for (int j = 0; j < tx->num_ops; j++) {
            Operation* op = &tx->ops[j];

            if (op->type == OP_TRANSFER) {
                printf("  op[%d] tick=%d %s from=%d to=%d amount=%d\n",
                    j, op->tick, op_name(op->type),
                    op->account_id, op->target_account, op->amount_centavos);
            } else if (op->type == OP_BALANCE) {
                printf("  op[%d] tick=%d %s account=%d\n",
                    j, op->tick, op_name(op->type), op->account_id);
            } else {
                printf("  op[%d] tick=%d %s account=%d amount=%d\n",
                    j, op->tick, op_name(op->type),
                    op->account_id, op->amount_centavos);
            }
        }
    }

    printf("\nTotal loaded transactions: %d\n\n", tx_printed);
}

static void print_loaded_accounts(Bank* bank) {
    printf("Loaded %d accounts:\n", bank->num_accounts);
    for (int i = 0; i < bank->num_accounts; i++) {
        printf("Account ID: %d, Balance: %d centavos\n", bank->accounts[i].account_id, bank->accounts[i].balance_centavos);
    }
}

// Configuration variables to store simulation settings
char *accounts_path = NULL;
char *trace_path = NULL;
int tick_interval_ms = 100; // Default from manual
char *deadlock_strategy = NULL;
bool verbose_logging = false;

// Use getopt to read input from terminal and assign to config variables
void parse_args(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"accounts", required_argument, 0, 'a'},
        {"trace",    required_argument, 0, 't'},
        {"deadlock", required_argument, 0, 'd'},
        {"tick-ms",  required_argument, 0, 'm'},
        {"verbose",  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    // getopt_long : handle flags without manually looping through argv[]
    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': accounts_path = optarg; break;
            case 't': trace_path = optarg; break;
            case 'd': deadlock_strategy = optarg; break;
            case 'm': {
                char* end = NULL;
                long parsed = strtol(optarg, &end, 10);
                if (end == optarg || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
                    fprintf(stderr, "Invalid --tick-ms value: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                tick_interval_ms = (int)parsed;
                break;
            }
            case 'v': verbose_logging = true; break;
        }
    }

    if (!accounts_path || !trace_path || !deadlock_strategy) {
        fprintf(stderr, "Usage: %s --accounts=FILE --trace=FILE --deadlock=prevention [--tick-ms=N] [--verbose]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(deadlock_strategy, "prevention") != 0) {
        fprintf(stderr, "Unsupported deadlock strategy: %s\n", deadlock_strategy);
        exit(EXIT_FAILURE);
    }
}

Bank* bank = NULL; // Global bank instance for now, remove when buffer pool is implemented
BufferPool buffer_pool;

int main(int argc, char* argv[]) {

    //parse CLI args for accounts file and trace file paths.
    // if (argc != 3) {
    //     fprintf(stderr, "Usage: %s <accounts_file> <trace_file>\n", argv[0]);   
    //     exit(EXIT_FAILURE);
    // }
    // char *accounts_path = argv[1];
    // char *trace_path = argv[2];
    // int tick_interval_ms = 1; // can make this configurable later if needed; hardcoded to 1 second per tick for now.
    
    parse_args(argc, argv);

    metrics_init();

    //initialize bank, accounts, and locks.  
    bank = create_bank(); 
    init_buffer_pool(&buffer_pool);

    // Use the variable populated by parse_args to populate bank struct
    if (load_accounts_file(bank, accounts_path)) {
        fprintf(stderr, "Failed to load accounts from %s\n", accounts_path);
        destroy_bank(bank);
        return 1;
    }


    int initial_sum = 0;
    for (int i = 0; i < bank->num_accounts; i++) {
        initial_sum += bank->accounts[i].balance_centavos;
    }

    //declare transactions array in main for now. NOT SURE if it should be added in the bank struct
    Transaction *txs[MAX_TRANSACTIONS];
    
    // File already loaded above
    // if (load_accounts_file(bank, accounts_path)){
    //     fprintf(stderr, "Failed to load accounts from %s\n", argv[1]);
    //     destroy_bank(bank);
    //     return 1;
    // }
    // load transaction trace
    int txs_count = load_transactions_file(trace_path, txs);
    if (txs_count < 0) {
        fprintf(stderr, "Failed to load transactions from %s\n", trace_path);
        destroy_bank(bank);
        return 1;    
    }


    if (verbose_logging) {
        print_loaded_accounts(bank);
        print_loaded_transactions(txs, MAX_TRANSACTIONS);
    }

    
    //initialize timer thread and logical clock
    pthread_t timer_tid; //TODO: put this in timer.c and make it static there since we don't need to access it outside of timer.c, and just have a timer_init() function that creates the thread and initializes the clock. Here for now we just declare it in main for simplicity.
    timer_init();
    if (pthread_create(&timer_tid, NULL, &timer_thread, &tick_interval_ms) != 0) {
        fprintf(stderr, "Failed to create timer thread\n");
        destroy_bank(bank);// TODO: maybe put this repetitive code to a cleanup function
        return 1;
    }

    // pthread_t txs_threads[txs_count];
    for (int i = 0; i < txs_count; i++){
        if (pthread_create(&txs[i]->thread, NULL, &execute_transaction, txs[i]) != 0) {
            fprintf(stderr, "Failed to create thread for transaction T%d\n", txs[i]->tx_id);
            //cleanup
            for (int j = 0; j < i; j++) {
                pthread_cancel(txs[j]->thread);
            }
            pthread_cancel(timer_tid);
            destroy_bank(bank);
            return 1;
        }
    }

    // Wait for all transactions to finish
    for (int i = 0; i < txs_count; i++) {
        if (pthread_join(txs[i]->thread, NULL) != 0) {
            fprintf(stderr, "Failed to join thread for transaction T%d\n", txs[i]->tx_id);
        }
    }
    
    // Set all_transactions_done to True to stop timer_thread's infinite while true loop
    timer_stop();

    // Wait for timer thread to exit
    if (pthread_join(timer_tid, NULL) != 0) {
        perror("Failed to join thread");
    }

    // Destroy mutex and condition variable
    timer_destroy();

    print_final_report(bank, txs, txs_count, initial_sum);

    // Create transaction threads, and execute transactions
    for (int i = 0; i < MAX_TRANSACTIONS; i++) {
        if (txs[i] != NULL) {
            free(txs[i]);
        }
    }

    destroy_buffer_pool(&buffer_pool);
    metrics_destroy();

    destroy_bank(bank);
    return 0;
}

