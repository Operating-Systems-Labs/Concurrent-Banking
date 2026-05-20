/* Declarations for lock manager APIs, lock ordering, and deadlock handling. */
#ifndef LOCK_MGR_H
#define LOCK_MGR_H

#include "bank.h"

typedef enum {
	LOCK_READ,
	LOCK_WRITE
} LockMode;

// Function declarations for lock management
void lock_account(Account* account, LockMode mode);
void unlock_account(Account* account);
void lock_accounts_ordered(Account* a, int a_id, Account* b, int b_id);
void unlock_accounts_ordered(Account* a, int a_id, Account* b, int b_id);

#endif // LOCK_MGR_H