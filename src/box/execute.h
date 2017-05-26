#ifndef TARANTOOL_SQL_EXECUTE_H_INCLUDED
#define TARANTOOL_SQL_EXECUTE_H_INCLUDED
/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct obuf;
struct sqlite3_stmt;
struct region;
enum mp_type;

/** Value of the SQL prepared statement parameter. */
struct sql_parameter {
	/** Parameter name. NULL for positioned parameters. */
	const char *name;
	/** Length of the @name. */
	uint32_t name_length;

	/** Length of the @s, if the @type is MP_STR or MP_BIN. */
	uint32_t bytes;
	/** MessagePack type of the value. */
	enum mp_type type;
	/** Parameter value. */
	union {
		double d;
		uint64_t u64;
		int64_t i64;
		/** For string and blob. */
		const char *s;
	};
};

/**
 * Decode SQL parameter values and names. Named and positioned
 * parameters are supported.
 * @param parameters MessagePack array of parameters without a
 *        header. Each parameter either must have scalar type, or
 *        must be a map with the following format: {name: value}.
 *        Name - string name of the named parameter,
 *        value - scalar value of the parameter. Named and
 *        positioned parameters can be mixed. For more details
 *        @sa https://www.sqlite.org/lang_expr.html#varparam.
 * @param parameter_count Length of @parameters.
 *
 * @retval not NULL Array of parameters with @parameter_count
 *         length.
 * @retval     NULL Client or memory error.
 */
struct sql_parameter *
sql_decode_parameters(const char *parameters, uint32_t parameter_count,
		      struct region *region);

/**
 * Steps:
 *   sql_prepare
 *        |
 *        v
 *     sql_bind <- - - - - - -+
 *        |                   |
 *        v                   |
 * sql_get_description        |
 *        |                   |
 *        v                   |
 *    sql_execute -> - - - - -+
 *        |
 *        V
 *   sql_finalize
 */

/**
 * Prepare an SQL query.
 * @param sql SQL query.
 * @param length Length of the @sql.
 * @param[out] prepared Result statement.
 *
 * @retval  0 Success.
 * @retval -1 Client or memory error.
 */
int
sql_prepare(const char *sql, uint32_t length, struct sqlite3_stmt **prepared);

/**
 * Bind parameter values to the prepared statement.
 * @param prepared Prepared statement.
 * @param parameters SQL query parameters.
 * @param parameter_count Count of @parameters.
 *
 * @retval  0 Success.
 * @retval -1 Client or memory error.
 */
int
sql_bind(struct sqlite3_stmt *prepared, const struct sql_parameter *parameters,
	 uint32_t parameter_count);

/**
 * Get description of the prepared statement.
 * @param prepared Prepared statement.
 * @param out Out buffer.
 * @param[out] count Count of description pairs.
 *
 * @retval  0 Success.
 * @retval -1 Client or memory error.
 */
int
sql_get_description(struct sqlite3_stmt *prepared, struct obuf *out,
		    uint32_t *count);

/**
 * Execute the prepred SQL query.
 * @param prepared Prepared statement.
 * @param out Out buffer.
 * @param[out] count Count of statements.
 *
 * @retval  0 Success.
 * @retval -1 Client or memory error.
 */
int
sql_execute(struct sqlite3_stmt *prepared, struct obuf *out, uint32_t *count);

/**
 * Free the prepared statement.
 * @param prepared Prepared statement.
 */
void
sql_finalize(struct sqlite3_stmt *prepared);

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* TARANTOOL_SQL_EXECUTE_H_INCLUDED */
