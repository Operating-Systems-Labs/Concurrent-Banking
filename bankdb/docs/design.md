# Design Notes

## Overview
This project implements a concurrent in-memory banking system using pthreads. Each transaction runs in its own thread, operations are scheduled by a global tick timer, and account access is synchronized with per-account reader-writer locks. A bounded buffer pool models account loading with semaphores.

## Build and Run
From the bankdb/ directory:

    make clean
    make
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --tick-ms=100

Manual build (without Makefile):

    gcc -Wall -Wextra -std=c11 -Iinclude src/*.c -o bin/bankdb -pthread
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --tick-ms=100

## Concurrency Model
- Timer thread advances a global tick and wakes waiting transactions.
- Each transaction thread waits for its start tick, then waits for each operation tick.
- Account access uses per-account pthread_rwlock_t to allow concurrent reads and exclusive writes.

## Deadlock Handling
- Deadlock prevention is enforced by lock ordering in transfers.
- Locks are acquired by ascending account ID to avoid circular wait.
- The CLI only supports prevention (no detection mode).

### Deadlock Strategy Choice
- Chosen strategy: prevention via lock ordering.
- Rationale: simpler to implement and avoids cycle detection overhead.
- Coffman condition broken: circular wait (global order on locks).

## Buffer Pool
- A fixed-size buffer pool (BUFFER_POOL_SIZE) simulates loading accounts.
- load_account() waits on empty_slots, marks a slot in use, then signals full_slots.
- unload_account() waits on full_slots, frees the slot, then signals empty_slots.
- Transactions call load_account/unload_account around each account access.

### Buffer Pool Integration
- Load/unload strategy: per operation (load before each account access, unload after).
- If the pool is full, load blocks on empty_slots until a slot is freed.
- Rationale: keeps pool pressure visible in traces and avoids long-lived slot hoarding.

## Metrics and Reporting
- metrics_init() creates the mutex used by metrics counters before any threads start.
- Deposits and withdrawals update total counters under a mutex.
- Final report prints the actual transaction status (COMMITTED or ABORTED).
- Buffer pool report prints total loads/unloads, peak usage, and blocked loads.

## Data Structures
- Bank contains a fixed array of Account records (MAX_ACCOUNTS) and a bank lock.
- Account includes id, balance, and a pthread_rwlock_t.
- Transactions store operations with per-op ticks and a start tick.

## Known Limitations
- Account lookup is linear by account_id, which is acceptable for lab scale but not optimal for large datasets.
- The bank instance and buffer pool are global for simplicity.

## Reader-Writer Lock Performance
- Benchmark setup: compare `pthread_mutex_t` vs `pthread_rwlock_t` on trace_readers.txt with `--tick-ms=10`.
- Results: rwlock End tick = 0; mutex End tick = 0.
- Explanation: with this small workload and 10ms ticks, both variants finish in the same tick, so the rwlock advantage does not show in the end-tick metric. The rwlock still preserves concurrent-reader semantics for larger or more contended runs.

## Timer Thread Design
- A separate timer thread enforces consistent tick progression across all transactions.
- Without it, operations would execute immediately and remove concurrent scheduling behavior.
- The timer thread enables deterministic scheduling based on trace ticks.

## ThreadSanitizer Notes
- Use `make debug` to build with ThreadSanitizer.
- Run `trace_readers.txt` with prevention and confirm zero warnings.
- Command:

    setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=100

## Additional Tests
- trace_abort_mid.txt: abort should stop later ops in the same transaction.
- trace_rw_mix.txt: concurrent reads with a writer on the same account.

## Test Commands
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --tick-ms=100
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=100
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --deadlock=prevention --tick-ms=100
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention --tick-ms=100
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --deadlock=prevention --tick-ms=100
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort_mid.txt --deadlock=prevention --tick-ms=100
    ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_rw_mix.txt --deadlock=prevention --tick-ms=100
