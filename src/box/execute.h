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

#if defined(__cplusplus)
extern "C" {
#endif

struct tuple;
struct port;
struct region;

/**
 * Execute SQL query and return result tuples with meta data.
 *
 * @param sql SQL query.
 * @param length Length of the @sql.
 * @param parameters SQL query parameters.
 *        @sa sqlite3_stmt_bind_msgpuck_parameters.
 * @param parameter_count Count of @parameters.
 * @param[out] description Meta information of the result rows.
 * @param port Storage for the result rows.
 * @param region Region memory allocator.
 *
 * @retval  0 Success.
 * @retval -1 Client or memory error.
 */
int
sql_execute(const char *sql, uint32_t length, const char *parameters,
	    uint32_t parameter_count, struct tuple **description,
	    struct port *port, struct region *region);

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* TARANTOOL_SQL_EXECUTE_H_INCLUDED */
