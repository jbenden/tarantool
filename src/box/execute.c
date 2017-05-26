#include "execute.h"
#include "box/iproto_constants.h"
#include "small/region.h"
#include "box/memtx_tuple.h"
#include "box/port.h"
#include "box/sql/sqlite3.h"
#include "sql.h"

/**
 * Get the meta information about the result of the prepared
 * statement. The meta is returned inside a memtx tuple.
 *
 * @param stmt   Prepared statement.
 * @param region Region memory allocator.
 *
 * @retval not NULL Meta information tuple.
 * @retval     NULL Memory error.
 */
static struct tuple *
get_sql_description(struct sqlite3_stmt *stmt, struct region *region)
{
	int column_count = sqlite3_column_count(stmt);
	assert(column_count > 0);
	size_t description_size = mp_sizeof_array(column_count);
	description_size += (mp_sizeof_map(1) +
			     mp_sizeof_uint(IPROTO_FIELD_NAME)) * column_count;
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
	size_t used = region_used(region);
	char *pos = (char *)region_alloc(region, description_size);
	if (pos == NULL) {
		diag_set(OutOfMemory, description_size, "region_alloc",
			 "description");
		return NULL;
	}
	char *begin = pos;

	pos = mp_encode_array(pos, column_count);
	for (int i = 0; i < column_count; ++i) {
		const char *name = sqlite3_column_name(stmt, i);
		pos = mp_encode_map(pos, 1);
		pos = mp_encode_uint(pos, IPROTO_FIELD_NAME);
		pos = mp_encode_str(pos, name, strlen(name));
	}
	struct tuple *ret = memtx_tuple_new(tuple_format_default, begin, pos);
	region_truncate(region, used);
	if (ret != NULL)
		tuple_ref(ret);
	return ret;
}

/**
 * Convert sqlite3 row into the tuple.
 *
 * @param stmt Started prepared statement. At least one
 *        sqlite3_step must be done.
 * @param region Region memory allocator.
 *
 * @retval not NULL Converted tuple.
 * @retval     NULL Memory error.
 */
static struct tuple *
sqlite3_stmt_to_tuple(struct sqlite3_stmt *stmt, struct region *region)
{
	int column_count = sqlite3_column_count(stmt);
	assert(column_count > 0);
	size_t size = mp_sizeof_array(column_count);
	for (int i = 0; i < column_count; ++i) {
		int type = sqlite3_column_type(stmt, i);
		switch(type) {
			case SQLITE_INTEGER: {
				int64_t n = sqlite3_column_int64(stmt, i);
				if (n >= 0)
					size += mp_sizeof_uint(n);
				else
					size += mp_sizeof_int(n);
				break;
			}
			case SQLITE_FLOAT:
				size += mp_sizeof_double(
						sqlite3_column_double(stmt, i));
				break;
			case SQLITE_TEXT:
				size += mp_sizeof_str(
						sqlite3_column_bytes(stmt, i));
				break;
			case SQLITE_BLOB:
				size += mp_sizeof_bin(
						sqlite3_column_bytes(stmt, i));
				break;
			case SQLITE_NULL:
				size += mp_sizeof_nil();
				break;
			default:
				unreachable();
		}
	}

	size_t used = region_used(region);
	char *pos = (char *)region_alloc(region, size);
	if (pos == NULL) {
		diag_set(OutOfMemory, size, "region_alloc", "raw tuple");
		return NULL;
	}
	char *begin = pos;
	pos = mp_encode_array(pos, column_count);
	for (int i = 0; i < column_count; ++i) {
		int type = sqlite3_column_type(stmt, i);
		switch(type) {
			case SQLITE_INTEGER: {
				int64_t n = sqlite3_column_int64(stmt, i);
				if (n >= 0)
					pos = mp_encode_uint(pos, n);
				else
					pos = mp_encode_int(pos, n);
				break;
			}
			case SQLITE_FLOAT:
				pos = mp_encode_double(pos,
						sqlite3_column_double(stmt, i));
				break;
			case SQLITE_TEXT:
				pos = mp_encode_str(pos,
					(const char *)sqlite3_column_text(stmt,
									  i),
					sqlite3_column_bytes(stmt, i));
				break;
			case SQLITE_BLOB:
				pos = mp_encode_bin(pos,
					sqlite3_column_blob(stmt, i),
					sqlite3_column_bytes(stmt, i));
				break;
			case SQLITE_NULL:
				pos = mp_encode_nil(pos);
				break;
			default:
				unreachable();
		}
	}
	struct tuple *ret = memtx_tuple_new(tuple_format_default, begin, pos);
	region_truncate(region, used);
	if (ret != NULL)
		tuple_ref(ret);
	return ret;
}

/**
 * Bind parameter values to their positions in the prepared
 * statement.
 *
 * @param db SQLite3 engine.
 * @param stmt Prepared statement.
 * @param parameters MessagePack array of parameters, without an
 *        array header. Each parameter either must have scalar
 *        type, or must be a map with the following format:
 *        {name: value}. Name - string name of the named
 *        parameter, value - scalar value of the parameter. Named
 *        and numbered parameters can be mixed. For more details
 *        @sa https://www.sqlite.org/lang_expr.html#varparam.
 * @param parameter_count Count of parameters.
 *
 * @retval  0 Success.
 * @retval -1 Client error.
 */
static int
sqlite3_stmt_bind_msgpuck_parameters(sqlite3 *db, struct sqlite3_stmt *stmt,
				     const char *parameters,
				     uint32_t parameter_count)
{
	assert(parameter_count > 0);
	assert(parameters != NULL);
	uint32_t pos = 1;
	for (uint32_t i = 0; i < parameter_count; pos = ++i + 1) {
		if (mp_typeof(*parameters) == MP_MAP) {
			uint32_t len = mp_decode_map(&parameters);
			if (len != 1 || mp_typeof(*parameters) != MP_STR)
				goto error;
			const char *name = mp_decode_str(&parameters, &len);
			pos = sqlite3_bind_parameter_lindex(stmt, name, len);
			if (pos == 0)
				goto error;
		}
		switch (mp_typeof(*parameters)) {
			case MP_UINT: {
				uint64_t n = mp_decode_uint(&parameters);
				if (n > INT64_MAX) {
					diag_set(ClientError, ER_UNSUPPORTED,
						 "SQL bind", "numbers greater "\
						 "than int64_max");
					return -1;
				}
				if (sqlite3_bind_int64(stmt, pos,
						       n) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_INT: {
				int64_t n = mp_decode_int(&parameters);
				if (sqlite3_bind_int64(stmt, pos,
						       n) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_STR: {
				uint32_t len;
				const char *s = mp_decode_str(&parameters,
							      &len);
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
				if (sqlite3_bind_text64(stmt, pos, s, len,
						      SQLITE_STATIC,
						      SQLITE_UTF8) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_DOUBLE: {
				double n = mp_decode_double(&parameters);
				if (sqlite3_bind_double(stmt, pos,
							n) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_FLOAT: {
				float n = mp_decode_float(&parameters);
				if (sqlite3_bind_double(stmt, pos,
							n) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_NIL: {
				mp_decode_nil(&parameters);
				if (sqlite3_bind_null(stmt, pos) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_BOOL: {
				/*
				 * SQLite doesn't really support
				 * boolean. Use int instead.
				 */
				int f = mp_decode_bool(&parameters) ? 1 : 0;
				if (sqlite3_bind_int(stmt, pos, f) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_BIN: {
				uint32_t len;
				const char *bin = mp_decode_bin(&parameters,
								&len);
				if (sqlite3_bind_blob64(stmt, pos,
						    (const void *)bin, len,
						    SQLITE_STATIC) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_EXT: {
				const char *start = parameters;
				mp_next(&parameters);
				if (sqlite3_bind_blob64(stmt, pos,
						    (const void *)start,
						    parameters - start,
						    SQLITE_STATIC) != SQLITE_OK)
					goto sql_error;
				break;
			}
			case MP_ARRAY:
			case MP_MAP:
				goto error;
			default:
				unreachable();
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
sql_execute(const char *sql, uint32_t length, const char *parameters,
	    uint32_t parameter_count, struct tuple **description,
	    struct port *port, struct region *region)
{
	assert(description != NULL);
	assert(port != NULL);
	*description = NULL;
	sqlite3 *db = sql_get();
	if (db == NULL) {
		diag_set(ClientError, ER_SQL, "sql processor is not ready");
		return -1;
	}
	int rc;
	int column_count = 0;
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, length, &stmt, &sql) != SQLITE_OK)
		goto sql_error;
	if (stmt == NULL)
		/* Empty request. */
		return 0;
	if (parameter_count != 0 &&
	    sqlite3_stmt_bind_msgpuck_parameters(db, stmt, parameters,
						 parameter_count) != 0) {
			goto error;
	}
	column_count = sqlite3_column_count(stmt);
	if (column_count == 0) {
		/*
		 * Query without response:
		 * CREATE/DELETE/INSERT ...
		 */
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW);
		if (rc != SQLITE_OK && rc != SQLITE_DONE)
			goto sql_error;
		sqlite3_finalize(stmt);
		return 0;
	}
	assert(column_count > 0);
	*description = get_sql_description(stmt, region);
	if (*description == NULL)
		goto error;

	while ((rc = sqlite3_step(stmt) == SQLITE_ROW)) {
		struct tuple *next = sqlite3_stmt_to_tuple(stmt, region);
		if (next == NULL)
			goto error;
		rc = port_add_tuple(port, next);
		tuple_unref(next);
		if (rc != 0)
			goto error;
	}
	if (rc != SQLITE_OK)
		goto sql_error;
	return 0;

sql_error:
	diag_set(ClientError, ER_SQL, sqlite3_errmsg(db));
error:
	sqlite3_finalize(stmt);
	if (*description != NULL)
		tuple_unref(*description);
	*description = NULL;
	return -1;
}
