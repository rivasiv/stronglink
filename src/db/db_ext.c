#include <assert.h>
#include <stdio.h>
#include "db_ext.h"

int db_get(DB_txn *const txn, DB_val *const key, DB_val *const data) {
	DB_cursor *cursor;
	int rc = db_txn_cursor(txn, &cursor);
	if(DB_SUCCESS != rc) return rc;
	return db_cursor_seek(cursor, key, data, 0);
}
int db_put(DB_txn *const txn, DB_val *const key, DB_val *const data, unsigned const flags) {
	DB_cursor *cursor;
	int rc = db_txn_cursor(txn, &cursor);
	if(DB_SUCCESS != rc) return rc;
	return db_cursor_put(cursor, key, data, flags);
}

int db_cursor_seekr(DB_cursor *const cursor, DB_range const *const range, DB_val *const key, DB_val *const data, int const dir) {
	int rc = db_cursor_seek(cursor, key, data, dir);
	if(DB_SUCCESS != rc) return rc;
	DB_val const *const limit = dir < 0 ? range->min : range->max;
	int x = db_cursor_cmp(cursor, key, limit);
	if(x * dir < 0) return DB_SUCCESS;
	db_cursor_clear(cursor);
	return DB_NOTFOUND;
}
int db_cursor_firstr(DB_cursor *const cursor, DB_range const *const range, DB_val *const key, DB_val *const data, int const dir) {
	if(0 == dir) return EINVAL;
	DB_val const *const first = dir > 0 ? range->min : range->max;
	DB_val k = *first;
	int rc = db_cursor_seek(cursor, &k, data, dir);
	if(DB_SUCCESS != rc) return rc;
	int x = db_cursor_cmp(cursor, first, &k);
	if(0 == x) {
		rc = db_cursor_next(cursor, &k, data, dir);
		if(DB_SUCCESS != rc) return rc;
	}
	DB_val const *const last = dir < 0 ? range->min : range->max;
	x = db_cursor_cmp(cursor, &k, last);
	if(x * dir < 0) {
		if(key) *key = k;
		return DB_SUCCESS;
	} else {
		db_cursor_clear(cursor);
		return DB_NOTFOUND;
	}
}
int db_cursor_nextr(DB_cursor *const cursor, DB_range const *const range, DB_val *const key, DB_val *const data, int const dir) {
	int rc = db_cursor_next(cursor, key, data, dir);
	if(DB_SUCCESS != rc) return rc;
	DB_val const *const limit = dir < 0 ? range->min : range->max;
	int x = db_cursor_cmp(cursor, key, limit);
	if(x * dir < 0) return DB_SUCCESS;
	db_cursor_clear(cursor);
	return DB_NOTFOUND;
}

