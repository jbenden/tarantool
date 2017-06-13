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
#include "backtrace.h"
#include "trivia/util.h"

#include <stdlib.h>
#include <stdio.h>

#include "say.h"
#include "fiber.h"

#define CRLF "\n"

#ifdef ENABLE_BACKTRACE

/*
 * We use a global static buffer because it is too late to do any
 * allocation when we are printing backtrace and fiber stack is
 * small.
 */

static char backtrace_buf[4096 * 4];

char *
backtrace(unw_context_t *unw_ctx)
{
	int frame_no = 0, status;
	unw_word_t sp, ip, offset, old_ip = 0;
	unw_cursor_t unw_cur;
	unw_init_local(&unw_cur, unw_ctx);
	char *p = backtrace_buf;
	char *end = p + sizeof(backtrace_buf) - 1;
	int unw_status;
	while ((unw_status = unw_step(&unw_cur)) > 0) {
		char proc[80];
		unw_get_reg(&unw_cur, UNW_REG_IP, &ip);
		if (ip == old_ip) {
			say_debug("unwinding error: previous frame "
				 "identical to this frame (corrupt stack?)");
			goto out;
		}
		old_ip = ip;
		unw_get_reg(&unw_cur, UNW_REG_SP, &sp);
		unw_get_proc_name(&unw_cur, proc, sizeof(proc), &offset);
		p += snprintf(p, end - p, "#%-2d %p in ", frame_no, (void *)ip);
		if (p >= end)
			goto out;
		{
			char *demangled = NULL;
			(void) status;
//			char *demangled = abi::__cxa_demangle(proc, 0, 0,
//							      &status);

			p += snprintf(p, end - p, "%s+%lx",
				      demangled != NULL ? demangled : proc,
				      (long)offset);
			free(demangled);
		}
		if (p >= end)
			goto out;
		p += snprintf(p, end - p, CRLF);
		if (p >= end)
			goto out;
		++frame_no;
	}
	if (unw_status != 0)
		say_debug("unwinding error: %s", unw_strerror(unw_status));
out:
	*p = '\0';
	return backtrace_buf;
}

void
backtrace_foreach(backtrace_cb cb, unw_context_t *unw_ctx, void *cb_ctx)
{
	int frame_no = 0, status;
	unw_word_t sp, ip, offset, old_ip = 0;
	unw_cursor_t unw_cur;
	unw_init_local(&unw_cur, unw_ctx);
	int unw_status;
	while ((unw_status = unw_step(&unw_cur)) > 0) {
		char proc[80];
		unw_get_reg(&unw_cur, UNW_REG_IP, &ip);
		if (ip == old_ip) {
			say_debug("unwinding error: previous frame "
				 "identical to this frame (corrupt stack?)");
			return;
		}
		old_ip = ip;
		unw_get_reg(&unw_cur, UNW_REG_SP, &sp);
		unw_get_proc_name(&unw_cur, proc, sizeof(proc), &offset);
		(void) status;
		char *demangled = NULL; /* abi::__cxa_demangle(proc, 0, 0,
						      &status);*/
		int rc = cb(frame_no, (void *)ip,
			    demangled != NULL ? demangled : proc,
			    offset, cb_ctx);
		free(demangled);
		if (rc != 0)
			return;
		++frame_no;
	}
	if (unw_status != 0)
		say_debug("unwinding error: %s", unw_strerror(unw_status));
}

void
print_backtrace()
{
	unw_context_t unw_ctx;
/* arm workaround around register asm variable declaration */
#define asm __asm__
	unw_getcontext((unw_context_t *)&unw_ctx);
#undef asm
	fdprintf(STDERR_FILENO, "%s", backtrace(&unw_ctx));
}
#endif /* ENABLE_BACKTRACE */


NORETURN void
assert_fail(const char *assertion, const char *file, unsigned int line, const char *function)
{
	fprintf(stderr, "%s:%i: %s: assertion %s failed.\n", file, line, function, assertion);
#ifdef ENABLE_BACKTRACE
	print_backtrace();
#endif /* ENABLE_BACKTRACE */
	close_all_xcpt(0);
	abort();
}

