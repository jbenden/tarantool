#ifndef INCLUDES_TARANTOOL_BOX_VY_STAT_H
#define INCLUDES_TARANTOOL_BOX_VY_STAT_H
/*
 * Copyright 2010-2017, Tarantool AUTHORS, please see AUTHORS file.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
#endif /* defined(__cplusplus) */

/** Used for accounting statements stored in memory. */
struct vy_stmt_counter {
	/** Number of statements. */
	int64_t rows;
	/** Size, in bytes. */
	int64_t bytes;
};

/** Used for accounting statements stored on disk. */
struct vy_disk_stmt_counter {
	/** Number of statements. */
	int64_t rows;
	/** Size when uncompressed, in bytes. */
	int64_t bytes;
	/** Size when compressed, in bytes */
	int64_t bytes_compressed;
	/** Number of pages. */
	int64_t pages;
};

/** Vinyl index statistics. */
struct vy_index_stat {
	/** Memory related statistics. */
	struct {
		/** Number of statements stored in memory. */
		struct vy_stmt_counter count;
	} memory;
	/** Disk related statistics. */
	struct {
		/** Number of statements stored on disk. */
		struct vy_disk_stmt_counter count;
	} disk;
};

static inline void
vy_stmt_counter_add(struct vy_stmt_counter *c1,
		    const struct vy_stmt_counter *c2)
{
	c1->rows += c2->rows;
	c1->bytes += c2->bytes;
}

static inline void
vy_stmt_counter_sub(struct vy_stmt_counter *c1,
		    const struct vy_stmt_counter *c2)
{
	c1->rows -= c2->rows;
	c1->bytes -= c2->bytes;
}

static inline void
vy_disk_stmt_counter_add(struct vy_disk_stmt_counter *c1,
			 const struct vy_disk_stmt_counter *c2)
{
	c1->rows += c2->rows;
	c1->bytes += c2->bytes;
	c1->bytes_compressed += c2->bytes_compressed;
	c1->pages += c2->pages;
}

static inline void
vy_disk_stmt_counter_sub(struct vy_disk_stmt_counter *c1,
			 const struct vy_disk_stmt_counter *c2)
{
	c1->rows -= c2->rows;
	c1->bytes -= c2->bytes;
	c1->bytes_compressed -= c2->bytes_compressed;
	c1->pages -= c2->pages;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* INCLUDES_TARANTOOL_BOX_VY_STAT_H */
