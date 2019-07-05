/*
 * Copyright (c) 2019 Carnegie Mellon University,
 * Copyright (c) 2019 Triad National Security, LLC, as operator of
 *     Los Alamos National Laboratory.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of CMU, TRIAD, Los Alamos National Laboratory, LANL, the
 *    U.S. Government, nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * shufapi.h: a 3-hop shuffle service atop deltafs-nexus and mercury-rpc.
 */
#pragma once

#include <mercury_types.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct deltafs_shuf;

typedef void (*deltafs_shuf_hdlr)(int src, int dst, uint32_t type, void* msg,
                                  uint32_t msglen);

struct deltafs_shuf_opts {
  char* subnet;
  char* proto;
  void* nxh;
  deltafs_shuf_hdlr hdlr;
  int lsenderlimit;
  int rsenderlimit;
  int lomaxrpc;
  int lobuftarget;
  int lrmaxrpc;
  int lrbuftarget;
  int rmaxrpc;
  int rbuftarget;
  int deliverq_max;
  int deliverq_min;
};

/*
 * deltafs_shuf_init: initialize the shuffle layer.  note that
 * lomaxrpc/lrmaxrpc/rmaxrpc is applied per dest, whereas
 * localsenderlimit/remotesenderlimit is applied to deltafs_shuf_enqueue
 * calls (but not relayed reqs) across all local (or remote) dests.
 * return a handle to shuffle context (a pointer) or NULL on error.
 */
struct deltafs_shuf* deltafs_shuf_init(const struct deltafs_shuf_opts* is);

/*
 * deltafs_shuf_localbarrier: perform a barrier operation across all processes
 * in my local communication group.  return 0 on success or -1 on error.
 */
int deltafs_shuf_localbarrier(struct deltafs_shuf* is);

/*
 * deltafs_shuf_localcommsz: return the number of processes (including me)
 * in my local communication group.
 */
int deltafs_shuf_localcommsz(struct deltafs_shuf* is);

/*
 * deltafs_shuf_localid: return my id in my local communication group.
 * if my local id is 0, I am a local master.
 */
int deltafs_shuf_localid(struct deltafs_shuf* is);

/*
 * deltafs_shuf_barrier: perform a barrier operation across all processes in the
 * global communication group.  return 0 on success or -1 on error.
 */
int deltafs_shuf_barrier(struct deltafs_shuf* is);

/*
 * deltafs_shuf_commsz: return the number of processes (including me) in the
 * global communication group.
 */
int deltafs_shuf_commsz(struct deltafs_shuf* is);

/*
 * deltafs_shuf_myid: return my id in the global communication group.
 * if my id is 0, I am a global master.
 */
int deltafs_shuf_myid(struct deltafs_shuf* is);

/*
 * deltafs_shuf_enqueue: start the sending of a message via the shuffle.
 * this is not end-to-end, it returns success once the message has
 * been queued for the next hop (the message data is copied into
 * the output queue, so the buffer passed in as an arg can be reused
 * when this function returns).  this function is expected to be called by the
 * main client thread.
 */
hg_return_t deltafs_shuf_enqueue(struct deltafs_shuf* is, int dst, void* msg,
                                 uint32_t msglen);

/*
 * deltafs_shuf_flushoriqs: flush all local origin queues.  this function blocks
 * until all requests currently in the specified output queues are delivered.
 * we make no claims about requests that arrive after the flush has been
 * started.
 */
hg_return_t deltafs_shuf_flushoriqs(struct deltafs_shuf* is);

/*
 * deltafs_shuf_flushremoteqs: flush all remote queues.  this function blocks
 * until all requests currently in the specified output queues are delivered.
 * we make no claims about requests that arrive after the flush has been
 * started.
 */
hg_return_t deltafs_shuf_flushremoteqs(struct deltafs_shuf* is);

/*
 * deltafs_shuf_flushrelayqs: flush all local relay queues.  this function
 * blocks until all requests currently in the specified output queues are
 * delivered. we make no claims about requests that arrive after the flush has
 * been started.
 */
hg_return_t deltafs_shuf_flushrelayqs(struct deltafs_shuf* is);

/*
 * deltafs_shuf_flushdeliv: flush the delivery queue.  this function
 * blocks until all requests currently in the delivery queues are
 * delivered.   we make no claims about requests that arrive after
 * the flush has been started.
 */
hg_return_t deltafs_shuf_flushdeliv(struct deltafs_shuf* is);

/*
 * deltafs_shuf_free: shutdown the shuffle layer and free resources.
 */
void deltafs_shuf_free(struct deltafs_shuf* is);

#ifdef __cplusplus
}
#endif
