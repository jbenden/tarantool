#ifndef TARANTOOL_APPLIER_H_INCLUDED
#define TARANTOOL_APPLIER_H_INCLUDED
/*
 * Copyright 2010-2015, Tarantool AUTHORS, please see AUTHORS file.
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

#include <netinet/in.h>
#include <sys/socket.h>

#include "trivia/util.h"
#include "uri.h"
#include "tt_uuid.h"
#include "trigger.h"
#include "third_party/tarantool_ev.h"
#include "vclock.h"
#include "ipc.h"
#include "small/mempool.h"
#include "diag.h"
#include "session.h"

struct xstream;

enum { APPLIER_SOURCE_MAXLEN = 1024 }; /* enough to fit URI with passwords */

#define applier_STATE(_)                                             \
	_(APPLIER_OFF, 0)                                            \
	_(APPLIER_CONNECT, 1)                                        \
	_(APPLIER_AUTH, 2)                                           \
	_(APPLIER_CONNECTED, 3)                                      \
	_(APPLIER_INITIAL_JOIN, 4)                                   \
	_(APPLIER_FINAL_JOIN, 5)                                     \
	_(APPLIER_JOINED, 6)                                         \
	_(APPLIER_FOLLOW, 7)                                         \
	_(APPLIER_STOPPED, 8)                                        \
	_(APPLIER_DISCONNECTED, 9)                                   \

/** States for the applier */
ENUM(applier_state, applier_STATE);
extern const char *applier_state_strs[];

/**
 * State of a replication connection to the master
 */
struct applier {
	struct fiber *reader;
	enum applier_state state;
	ev_tstamp lag, last_row_time;
	bool warning_said;
	uint32_t id;
	struct tt_uuid uuid;
	char source[APPLIER_SOURCE_MAXLEN];
	struct uri uri;
	uint32_t version_id; /* remote version */
	struct vclock vclock;
	union {
		struct sockaddr addr;
		struct sockaddr_storage addrstorage;
	};
	socklen_t addr_len;
	struct ev_io io;
	/** Input/output buffers for buffered IO */
	struct iobuf *iobuf[2];
	/** Index of current input buffer */
	int input_index;
	/** Amount of unparsed input bytes */
	size_t input_unparsed;
	/** True if applier fiber want swap buffers */
	bool want_swap_buffers;
	/** Mempool for recovery messages */
	struct mempool msg_pool;
	/** Triggers invoked on state change */
	struct rlist on_state;
	/* Channel used by applier_connect_all() and applier_resume() */
	struct ipc_channel pause;
	struct xstream *initial_join_stream;
	struct xstream *final_join_stream;
	struct xstream *subscribe_stream;
	/* Session for applier */
	struct session *session;
	/* If tx processing failed we will setup this diag
	 * and cancel applier fiber. */
	struct diag cancel_reason;
	/* If tx want some state from applier but have some error then
	 * tx will "steal" diag from applier and raise exception in tx.
	 * In this case we souldn't rethrow error in applier fiber. */
	bool fiber_nothrow;
	/* If true all request processing should be silently skipped.
	 * For example, in case of socket errors we need to flush applier
	 * queue before new connection attempt. */
	bool drop_requests;
};

/**
 * Stop a client.
 */
void
tx_applier_stop(struct applier *applier);

/**
 * Allocate an instance of applier object, create applier and initialize
 * remote uri (copied to struct applier).
 *
 * @pre     the uri is a valid and checked one
 * @error   throws OutOfMemory exception if out of memory.
 */
struct applier *
applier_new(const char *uri, struct xstream *initial_join_stream,
	    struct xstream *final_join_stream,
	    struct xstream *subscribe_stream);

/**
 * Destroy and delete a applier.
 */
void
tx_applier_delete(struct applier *applier);

/*
 * Connect all appliers to remote peer and receive UUID
 * \post appliers are connected to remote hosts and paused.
 * Use applier_resume(applier) to resume applier.
 */
void
tx_applier_connect_all(struct applier **appliers, int count);

/*
 * Resume execution of applier until \a state.
 */
void
tx_applier_resume_to_state(struct applier *applier, enum applier_state state,
			double timeout);

/*
 * Resume execution of applier.
 */
void
tx_applier_resume(struct applier *applier);

#endif /* TARANTOOL_APPLIER_H_INCLUDED */
