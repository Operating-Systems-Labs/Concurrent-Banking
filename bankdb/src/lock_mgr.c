/* Lock acquisition/release flow and deadlock prevention or detection logic. */
#include "lock_mgr.h"

void lock_account(Account* account, LockMode mode) {
	if (!account) {
		return;
	}

	if (mode == LOCK_READ) {
		pthread_rwlock_rdlock(&account->lock);
	} else {
		pthread_rwlock_wrlock(&account->lock);
	}
}

void unlock_account(Account* account) {
	if (!account) {
		return;
	}

	pthread_rwlock_unlock(&account->lock);
}

void lock_accounts_ordered(Account* a, int a_id, Account* b, int b_id) {
	if (!a || !b) {
		return;
	}

	if (a_id <= b_id) {
		lock_account(a, LOCK_WRITE);
		lock_account(b, LOCK_WRITE);
	} else {
		lock_account(b, LOCK_WRITE);
		lock_account(a, LOCK_WRITE);
	}
}

void unlock_accounts_ordered(Account* a, int a_id, Account* b, int b_id) {
	if (!a || !b) {
		return;
	}

	if (a_id <= b_id) {
		unlock_account(b);
		unlock_account(a);
	} else {
		unlock_account(a);
		unlock_account(b);
	}
}
