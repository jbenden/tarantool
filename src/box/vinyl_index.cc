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
#include "vinyl_index.h"

#include <stdio.h>

#include "trivia/util.h"
#include "scoped_guard.h"
#include "schema.h"
#include "txn.h"
#include "vinyl.h"
#include "tuple.h"

/**
 * Get (struct vy_index *) by (struct Index *).
 * @param index VinylIndex to convert.
 * @retval Pointer to index->db.
 */
extern "C" struct vy_index *
vy_index(struct Index *index)
{
	return ((struct VinylIndex *) index)->db;
}

struct vinyl_iterator {
	struct iterator base;
	const VinylIndex *index;
	struct vy_cursor *cursor;
};

VinylIndex::VinylIndex(struct index_def *index_def, struct vy_index *db)
	: Index(index_def), db(db)
{
	vy_index_ref(db);
}

VinylIndex::~VinylIndex()
{
	vy_index_unref(db);
}

void
VinylIndex::open()
{
	if (vy_index_open(db) != 0)
		diag_raise();
}

void
VinylIndex::commitCreate()
{
	vy_index_commit_create(db);
}

void
VinylIndex::commitDrop()
{
	vy_index_commit_drop(db);
}

struct tuple*
VinylIndex::findByKey(const char *key, uint32_t part_count) const
{
	assert(index_def->opts.is_unique && part_count == index_def->key_def.part_count);
	/*
	 * engine_tx might be empty, even if we are in txn context.
	 * This can happen on a first-read statement.
	 */
	struct vy_tx *transaction = in_txn() ?
		(struct vy_tx *) in_txn()->engine_tx : NULL;
	struct tuple *tuple = NULL;
	if (vy_get(transaction, db, key, part_count, &tuple) != 0)
		diag_raise();
	if (tuple != NULL) {
		tuple = tuple_bless_xc(tuple);
		tuple_unref(tuple);
	}
	return tuple;
}

size_t
VinylIndex::bsize() const
{
	return vy_index_bsize(db);
}

struct tuple *
VinylIndex::min(const char *key, uint32_t part_count) const
{
	struct iterator *it = allocIterator();
	auto guard = make_scoped_guard([=]{it->free(it);});
	initIterator(it, ITER_GE, key, part_count);
	return it->next(it);
}

struct tuple *
VinylIndex::max(const char *key, uint32_t part_count) const
{
	struct iterator *it = allocIterator();
	auto guard = make_scoped_guard([=]{it->free(it);});
	initIterator(it, ITER_LE, key, part_count);
	return it->next(it);
}

size_t
VinylIndex::count(enum iterator_type type, const char *key,
		  uint32_t part_count) const
{
	struct iterator *it = allocIterator();
	auto guard = make_scoped_guard([=]{it->free(it);});
	initIterator(it, type, key, part_count);
	size_t count = 0;
	struct tuple *tuple = NULL;
	while ((tuple = it->next(it)) != NULL)
		++count;
	return count;
}

static struct tuple *
vinyl_iterator_last(MAYBE_UNUSED struct iterator *ptr)
{
	return NULL;
}

static inline struct tuple *
iterator_next(struct iterator *base_it)
{
	struct vinyl_iterator *it = (struct vinyl_iterator *) base_it;
	struct tuple *tuple;

	/* found */
	if (vy_cursor_next(it->cursor, &tuple) != 0) {
		/* immediately close the cursor */
		vy_cursor_delete(it->cursor);
		it->cursor = NULL;
		it->base.next = vinyl_iterator_last;
		diag_raise();
	}
	if (tuple != NULL) {
		tuple = tuple_bless_xc(tuple);
		tuple_unref(tuple);
		return tuple;
	}

	/* immediately close the cursor */
	vy_cursor_delete(it->cursor);
	it->cursor = NULL;
	it->base.next = vinyl_iterator_last;
	return NULL;
}

static void
vinyl_iterator_free(struct iterator *ptr)
{
	assert(ptr->free == vinyl_iterator_free);
	struct vinyl_iterator *it = (struct vinyl_iterator *) ptr;
	if (it->cursor) {
		vy_cursor_delete(it->cursor);
		it->cursor = NULL;
	}
	free(ptr);
}

struct iterator*
VinylIndex::allocIterator() const
{
	struct vinyl_iterator *it =
	        (struct vinyl_iterator *) calloc(1, sizeof(*it));
	if (it == NULL) {
	        tnt_raise(OutOfMemory, sizeof(struct vinyl_iterator),
	                  "calloc", "vinyl_iterator");
	}
	it->base.next = vinyl_iterator_last;
	it->base.free = vinyl_iterator_free;
	return (struct iterator *) it;
}

void
VinylIndex::initIterator(struct iterator *ptr,
                         enum iterator_type type,
                         const char *key, uint32_t part_count) const
{
	assert(part_count == 0 || key != NULL);
	struct vinyl_iterator *it = (struct vinyl_iterator *) ptr;
	struct vy_tx *tx =
		in_txn() ? (struct vy_tx *) in_txn()->engine_tx : NULL;
	assert(it->cursor == NULL);
	it->index = this;
	ptr->next = iterator_next;
	if (type > ITER_GT || type < 0)
		return Index::initIterator(ptr, type, key, part_count);

	it->cursor = vy_cursor_new(tx, db, key, part_count, type);
	if (it->cursor == NULL)
		diag_raise();
}

void
VinylIndex::info(struct info_handler *handler) const
{
	vy_index_info(db, handler);
}
