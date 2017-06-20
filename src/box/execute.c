#include "box/iproto_constants.h"
#include "box/sql/sqlite3.h"
#include "box/errcode.h"
#include "small/region.h"
#include "small/obuf.h"
#include "execute.h"
#include "diag.h"
#include "sql.h"

/**
 * Encode sqlite3 row into an obuf using MessagePack.
 * @param stmt Started prepared statement. At least one
 *        sqlite3_step must be done.
 * @param out Out buffer.
 *
 * @retval  0 Success.
 * @retval -1 Memory error.
 */
static int
sqlite3_stmt_to_obuf(struct sqlite3_stmt *stmt, struct obuf *out)
{
	int column_count = sqlite3_column_count(stmt);
	assert(column_count > 0);
	size_t size = mp_sizeof_array(column_count);
	char *pos = (char *) obuf_alloc(out, size);
	if (pos == NULL)
		goto out_of_mem_err;
	mp_encode_array(pos, column_count);

	for (int i = 0; i < column_count; ++i) {
		int type = sqlite3_column_type(stmt, i);
		switch(type) {
			case SQLITE_INTEGER: {
				int64_t n = sqlite3_column_int64(stmt, i);
				if (n >= 0)
					size = mp_sizeof_uint(n);
				else
					size = mp_sizeof_int(n);
				pos = (char *) obuf_alloc(out, size);
				if (pos == NULL)
					goto out_of_mem_err;
				if (n >= 0)
					mp_encode_uint(pos, n);
				else
					mp_encode_int(pos, n);
				break;
			}
			case SQLITE_FLOAT: {
				double d = sqlite3_column_double(stmt, i);
				size = mp_sizeof_double(d);
				pos = (char *) obuf_alloc(out, size);
				if (pos == NULL)
					goto out_of_mem_err;
				mp_encode_double(pos, d);
				break;
			}
			case SQLITE_TEXT: {
				uint32_t len = sqlite3_column_bytes(stmt, i);
				size = mp_sizeof_str(len);
				pos = (char *) obuf_alloc(out, size);
				if (pos == NULL)
					goto out_of_mem_err;
				const char *s =
					(const char *)sqlite3_column_text(stmt,
									  i);
				mp_encode_str(pos, s, len);
				break;
			}
			case SQLITE_BLOB: {
				uint32_t len = sqlite3_column_bytes(stmt, i);
				size = mp_sizeof_bin(len);
				pos = (char *) obuf_alloc(out, size);
				if (pos == NULL)
					goto out_of_mem_err;
				const char *s =
					(const char *)sqlite3_column_blob(stmt,
									  i);
				mp_encode_bin(pos, s, len);
				break;
			}
			case SQLITE_NULL: {
				size = mp_sizeof_nil();
				pos = (char *) obuf_alloc(out, size);
				if (pos == NULL)
					goto out_of_mem_err;
				mp_encode_nil(pos);
				break;
			}
			default:
				unreachable();
		}
	}
	return 0;
out_of_mem_err:
	diag_set(OutOfMemory, size, "obuf_alloc", "pos");
	return -1;
}

struct sql_parameter *
sql_decode_parameters(const char *parameters, uint32_t parameter_count,
		      struct region *region)
{
	assert(parameter_count > 0);
	assert(parameters != NULL);
	uint32_t used = region_used(region);
	size_t size = sizeof(struct sql_parameter) * parameter_count;
	struct sql_parameter *p =
		(struct sql_parameter *) region_alloc(region, size);
	if (p == NULL) {
		diag_set(OutOfMemory, size, "region_alloc", "p");
		return NULL;
	}
	for (uint32_t i = 0; i < parameter_count; ++i) {
		if (mp_typeof(*parameters) == MP_MAP) {
			uint32_t len = mp_decode_map(&parameters);
			/*
			 * Named parameter consists of MP_MAP with
			 * one key - {'str_name': value}.
			 * Else error.
			 */
			if (len != 1 || mp_typeof(*parameters) != MP_STR) {
				diag_set(ClientError, ER_SQL_ILLEGAL_BIND,
					 (unsigned)i + 1);
				goto error;
			}
			p[i].name = mp_decode_str(&parameters,
						  &p[i].name_length);
		} else {
			p[i].name = NULL;
		}
		p[i].type = mp_typeof(*parameters);
		switch (p[i].type) {
			case MP_UINT:
				p[i].u64 = mp_decode_uint(&parameters);
				if (p[i].u64 > INT64_MAX) {
					diag_set(ClientError, ER_UNSUPPORTED,
						 "SQL bind", "numbers greater "\
						 "than int64_max");
					goto error;
				}
				break;
			case MP_INT:
				p[i].i64 = mp_decode_int(&parameters);
				break;
			case MP_STR:
				p[i].s = mp_decode_str(&parameters,
						       &p[i].bytes);
				break;
			case MP_DOUBLE:
				p[i].d = mp_decode_double(&parameters);
				break;
			case MP_FLOAT:
				p[i].d = mp_decode_float(&parameters);
				break;
			case MP_NIL:
				mp_decode_nil(&parameters);
				break;
			case MP_BOOL:
				/*
				 * SQLite doesn't really support
				 * boolean. Use int instead.
				 */
				p[i].u64 = mp_decode_bool(&parameters) ? 1 : 0;
				p[i].type = MP_UINT;
				break;
			case MP_BIN:
				p[i].s = mp_decode_bin(&parameters,
						       &p[i].bytes);
				break;
			case MP_EXT:
				p[i].s = parameters;
				mp_next(&parameters);
				p[i].bytes = parameters - p[i].s;
				break;
			case MP_ARRAY:
			case MP_MAP:
				diag_set(ClientError, ER_SQL_ILLEGAL_BIND,
					 (unsigned)i + 1);
				goto error;
			default:
				unreachable();
		}
	}
	return p;

error:
	region_truncate(region, used);
	return NULL;
}

int
sql_prepare(const char *sql, uint32_t length, struct sqlite3_stmt **prepared)
{
	*prepared = NULL;
	sqlite3 *db = sql_get();
	if (db == NULL) {
		diag_set(ClientError, ER_SQL, "sql processor is not ready");
		return -1;
	}
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, length, &stmt, &sql) != SQLITE_OK) {
		diag_set(ClientError, ER_SQL, sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	*prepared = stmt;
	return 0;
}

int
sql_bind(struct sqlite3_stmt *stmt, const struct sql_parameter *p,
	 uint32_t parameter_count)
{
	sqlite3 *db = sql_get();
	if (db == NULL) {
		diag_set(ClientError, ER_SQL, "sql processor is not ready");
		return -1;
	}
	assert(parameter_count > 0);
	assert(p != NULL);
	uint32_t pos = 1;
	for (uint32_t i = 0; i < parameter_count; pos = ++i + 1) {
		if (p[i].name != NULL) {
			pos = sqlite3_bind_parameter_lindex(stmt, p[i].name,
							    p[i].name_length);
			if (pos == 0)
				goto error;
		}
		switch (p[i].type) {
			case MP_UINT:
				assert(p[i].u64 <= INT64_MAX);
				if (sqlite3_bind_int64(stmt, pos,
						       p[i].u64) != SQLITE_OK)
					goto sql_error;
				break;
			case MP_INT:
				if (sqlite3_bind_int64(stmt, pos,
						       p[i].i64) != SQLITE_OK)
					goto sql_error;
				break;
			case MP_STR:
				/*
				 * Parameters are allocated within
				 * message pack, received from the
				 * iproto thread. IProto thread
				 * now is waiting for the response
				 * and it will not free the
				 * parameters until
				 * sqlite3_finalize. So there is
				 * no need to copy the parameters
				 * and we can use SQLITE_STATIC.
				 */
				if (sqlite3_bind_text64(stmt, pos, p[i].s,
						      p[i].bytes,
						      SQLITE_STATIC,
						      SQLITE_UTF8) != SQLITE_OK)
					goto sql_error;
				break;
			case MP_DOUBLE:
			case MP_FLOAT:
				if (sqlite3_bind_double(stmt, pos,
							p[i].d) != SQLITE_OK)
					goto sql_error;
				break;
			case MP_NIL:
				if (sqlite3_bind_null(stmt, pos) != SQLITE_OK)
					goto sql_error;
				break;
			case MP_BIN:
			case MP_EXT:
				if (sqlite3_bind_blob64(stmt, pos,
						    (const void *) p[i].s,
						    p[i].bytes,
						    SQLITE_STATIC) != SQLITE_OK)
					goto sql_error;
				break;
			case MP_ARRAY:
			case MP_MAP:
				goto error;
			default: {
				/*
				 * Including MP_BOOL - it is
				 * turned into MP_INT.
				 */
				unreachable();
			}
		}
	}
	return 0;

error:
	diag_set(ClientError, ER_SQL_ILLEGAL_BIND, (unsigned)pos);
	return -1;
sql_error:
	diag_set(ClientError, ER_SQL, sqlite3_errmsg(db));
	return -1;
}

int
sql_get_description(struct sqlite3_stmt *stmt, struct obuf *out,
		    uint32_t *count)
{
	assert(count != NULL);
	int column_count = sqlite3_column_count(stmt);
	size_t description_size =
		mp_sizeof_map(1) + mp_sizeof_uint(IPROTO_FIELD_NAME);
	description_size *= column_count;
	for (int i = 0; i < column_count; ++i) {
		const char *name = sqlite3_column_name(stmt, i);
		/*
		 * Can not fail, since all column names are
		 * preallocated during prepare phase and the
		 * column_name simply returns them.
		 */
		assert(name != NULL);
		description_size += mp_sizeof_str(strlen(name));
	}
	char *pos = (char *) obuf_alloc(out, description_size);
	if (pos == NULL) {
		diag_set(OutOfMemory, description_size, "obuf_alloc", "pos");
		return -1;
	}
	*count = (uint32_t) column_count;
	for (int i = 0; i < column_count; ++i) {
		const char *name = sqlite3_column_name(stmt, i);
		pos = mp_encode_map(pos, 1);
		pos = mp_encode_uint(pos, IPROTO_FIELD_NAME);
		pos = mp_encode_str(pos, name, strlen(name));
	}
	return 0;
}

int
sql_execute(struct sqlite3_stmt *prepared, struct obuf *out, uint32_t *count)
{
	assert(count != NULL);
	assert(prepared != NULL);
	sqlite3 *db = sql_get();
	if (db == NULL) {
		diag_set(ClientError, ER_SQL, "sql processor is not ready");
		return -1;
	}
	*count = 0;
	int column_count = sqlite3_column_count(prepared);
	int rc;
	if (column_count == 0) {
		/*
		 * Query without response:
		 * CREATE/DELETE/INSERT ...
		 */
		while ((rc = sqlite3_step(prepared)) == SQLITE_ROW);
		if (rc != SQLITE_OK && rc != SQLITE_DONE)
			goto sql_error;
		return 0;
	}
	assert(column_count > 0);
	while ((rc = sqlite3_step(prepared) == SQLITE_ROW)) {
		if (sqlite3_stmt_to_obuf(prepared, out) != 0)
			return -1;
		++*count;
	}
	return rc == SQLITE_OK ? 0 : -1;

sql_error:
	diag_set(ClientError, ER_SQL, sqlite3_errmsg(db));
	return -1;
}

void
sql_finalize(struct sqlite3_stmt *prepared)
{
	sqlite3_finalize((sqlite3_stmt *) prepared);
}
