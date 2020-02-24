/*
 * Copyright (c) 2017-2019 Carnegie Mellon University,
 * Copyright (c) 2017-2019 Triad National Security, LLC, as operator of
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
 * shuffle_api.h  3-hop shuffle service atop deltafs-nexus and mercury RPC
 */

/*
 * the 3-hop shuffle uses mercury RPCs to send a message from a SRC process
 * to a DST process.  our goal is to reduce per-process memory usage by
 * reducing the number of output queues and connections to manage on
 * each node (we are assuming an environment where each node has
 * multiple CPUs and there is an app with processes running on each
 * CPU).
 *
 * as an example, consider LANL Trinity nodes: there are 10,000 haswell
 * nodes with 32 cores on each node.   an application running on these nodes
 * would have 320,000 processes (10,000 nodes * 32 cores/node).  if
 * all processes communicate with each other, each process will need
 * memory for ~320,000 output queues!
 *
 * to reduce the memory usage, we add layers of indirection to the system
 * (in the form of additional mercury RPC hops).   the result is a 3
 * hop shuffle:
 *
 *  SRC  ---na+sm--->   SRCREP ---network--->   DSTREP   ---na+sm--->  DST
 *            1                      2                        3
 *
 * note: "na+sm" is mercury's shared memory transport, "REP" == representative
 *
 * we divide the job for sending to each remote node among the local
 * cores.  furthermore, we only send to one process on each remote node.
 * we expect the remote receiving process to forward our message to
 * the final destination (if it isn't the final destination).
 *
 * thus, on Trinity, each SRC process has 31 na+sm output queues to
 * talk to other local processes, and each SRC process has 10,000/32
 * (~313) network output queues to talk to the remote nodes it is
 * responsible for.   this is much less than the ~320,000 output queues
 * needed in the all-to-all case.
 *
 * a msg from a SRC to a remote DST on node N flows like this:
 *  1. SRC find the local proc responsible for talking to N.  this is
 *     the SRCREP.   it forward the msg to the SRCREP over na+sm.
 *     this first local hop is the origin or client hop.
 *  2. the SRCREP forwards all messages for node N to one process on
 *     node N over the network.   this is the DSTREP.
 *  3. the DSTREP receives the message and looks for its na+sm connection
 *     to the DST (which is also on node N) and sends the msg to DST.
 *     this final local hop is the relay hop (DSTREP locally relays the
 *     msg to the final DST process via mercury)
 * once the msg reaches the DST it can be delivered to the application.
 * note that it is possible to skip hops (e.g. if SRC==SRCREP, the first
 * hop can be skipped).
 *
 * the shuffle library manages this three hop communication.  it
 * has support for batching multiple messages into a larger batch
 * message (to reduce the overhead of small writes), and it also
 * supports write-behind buffering (so that the application can
 * queue data in the buffer and continue, rather than waiting for
 * the RPC).  if/when the buffers fill, the library provides flow
 * control to stop the application from overfilling the buffers.
 *
 * for output queues we provide:
 *  - maxrpc:    the max number of outstanding RPCs we allow at one time.
 *               additional requests are queued on a wait list until
 *               something completes.
 *  - buftarget: controls batching of requests... we batch multiple
 *               requests into a single RPC until we have at least
 *               "buftarget" bytes in the batch.  setting this value
 *               small effectively disables batching.
 *
 * for delivery, we have:
 *  - deliverq_max:       the max number of delivery requests we will buffer
 *                        before we start putting additional requests on the
 *                        waitq (waitq requests are not ack'd until space
 *                        is available... this triggers flow control).
 *  - deliverq_threshold: we delay waking up the delivery thread until
 *                        more than threshold reqs are on the deliveryq.
 *                        this allows requests to be delivered in larger
 *                        batches (to avoid context switching overhead
 *                        when the request size is small).
 *
 * note that we identify endpoints by a global rank number.
 * 3 hop routing info is provided by deltafs-nexus (internally
 * nexus uses MPI to determine the topology, rank numbers, and
 * provide collective ops [barriers]).   see the nexus headers
 * for nexus API details.  shuffle itself does not directly call
 * MPI.
 *
 * we also support a broadcast mode where we use the 3 hop topology
 * to replicate data as it travels through the system.   we use the
 * top bit of the request "type" to indicate requests that are being
 * sent via broadcast.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <deltafs-nexus/deltafs-nexus_api.h> /* for nexus_ctx_t */
#include <mercury_types.h>                   /* for hg_return_t */

/*
 * shuffle_opts: passed to shuffle_init() to configure the shuffle's
 * flow control and batching/queueing options.
 */
struct shuffle_opts {
  int localsenderlimit;   /* max# of local RPCs we allow */
  int remotesenderlimit;  /* max# of remote RPCs we allow */
  int lomaxrpc;           /* max# of local origin/client RPCs for a dest */
  int lobuftarget;        /* target #bytes for local origin/client RPC */
  int lrmaxrpc;           /* max# of local relay RPCs for a dest */
  int lrbuftarget;        /* target #bytes for local relay RPC */
  int rmaxrpc;            /* max# of remote RPCs for a dest */
  int rbuftarget;         /* target #bytes for remote RPC */
  int deliverq_max;       /* max# requests in delivery q before flow ctrl */
  int deliverq_threshold; /* wake delivery thread when threshold# reqs q'd */
};

/*
 * shuffle_t: handle to shuffle state (a pointer)
 */
typedef struct shuffle *shuffle_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * shuffle_deliverfn_t: pointer to a callback function used to
 * deliver a msg to the DST.  this function may block if the
 * DST is busy/full.
 */
typedef void (*shuffle_deliverfn_t)(int src, int dst, uint32_t type,
                                    void *d, uint32_t datalen);

/*
 * shuffle_opts_init: init all values in an opts structures to the defaults
 *
 * @param sopt shuffle_opts to init
 */
void shuffle_opts_init(struct shuffle_opts *sopt);

/*
 * shuffle_init: inits the shuffle layer.  if this returns an
 * error, you'll need to shutdown and reinit mercury and nexus to
 * retry...  note that opts' lomaxrpc/lrmaxrpc/rmaxrpc is applied per dest,
 * while localsenderlimit/remotesenderlimit is applied to shuffle_send
 * calls (but not relayed reqs) across all local (or remote) dests.
 *
 * @param nxp the nexus context (routing info, already init'd)
 * @param funname rpc function name (for making a mercury RPC id number)
 * @param delivercb application callback to deliver data
 * @param sopt shuffle options
 * @return handle to shuffle (a pointer) or NULL on error
 */
shuffle_t shuffle_init(nexus_ctx_t nxp, char *funname,
          shuffle_deliverfn_t delivercb, struct shuffle_opts *sopt);

/*
 * defines for request types
 */
#define SHUFFLE_RTYPE_BCAST   (1 << 31)  /* req is a broadcast */
#define SHUFFLE_RTYPE_USRBITS 0x8fffffff /* user-defined bits */

/*
 * shuffle_enqueue: start the sending of a message via the shuffle.
 * this is not end-to-end, it returns success once the message has
 * been queued for the next hop (the message data is copied into
 * the output queue, so the buffer passed in as an arg can be reused
 * when this function returns).   this is called by the main client
 * thread.
 *
 * @param sh shuffle service handle
 * @param dst target to send to
 * @param type message type (normally 0)
 * @param d data buffer
 * @param datalen length of data
 * @return status (success if we've queued the data)
 */
hg_return_t shuffle_enqueue(shuffle_t sh, int dst, uint32_t type,
                            void *d, uint32_t datalen);

/*
 * broadcast flag bits
 */
#define SHUFFLE_BCAST_SELF 1  /* send a copy to our local delivery thread */

/*
 * shuffle_enqueue_broadcast: start the sending of a broadcast message
 * via the shuffle (and the 3 hop topology).  Note that internally we
 * convert the broadcast into normal RPCs.  In the unlikely event of
 * a failure, it is possible for the broadcast to only partially complete.
 *
 * @param sh shuffle service handle
 * @param type message type (normally 0)
 * @param d data buffer
 * @param datalen length of data
 * @param flags currently we have bcast_self
 * @return status (success if we've queued the data)
 */
hg_return_t shuffle_enqueue_broadcast(shuffle_t sh, uint32_t type, void *d,
                                      uint32_t datalen, int flags);

/*
 * shuffle_flush_delivery: flush the delivery queue.  this function
 * blocks until all requests currently in the delivery queues are
 * delivered.   We make no claims about requests that arrive after
 * the flush has been started.
 *
 * @param sh shuffle service handle
 * @return status
 */
hg_return_t shuffle_flush_delivery(shuffle_t sh);

/*
 * defines for which queue set to flush
 */
#define SHUFFLE_REMOTE_QUEUES 0    /* network queues (between nodes) */
#define SHUFFLE_ORIGIN_QUEUES 1    /* origin/client queues (local, na+sm) */
#define SHUFFLE_RELAY_QUEUES  2    /* relay queues (local, na+sm) */

/*
 * shuffle_flush_qs: flush the specified output queues.
 * this function blocks until all requests currently in the specified
 * output queues are delivered. We make no claims about requests that
 * arrive after the flush has been started.
 *
 * @param sh shuffle service handle
 * @param whichqs which queues to flush (see defines above)
 * @return status
 */
hg_return_t shuffle_flush_qs(shuffle_t sh, int whichqs);


/*
 * shuffle_flush_originqs: flush client/origin qs (wrap for shuffle_flush_qs)
 *
 * @param sh shuffle service handle
 * @return status
 */
#define shuffle_flush_originqs(S) \
        shuffle_flush_qs((S), SHUFFLE_ORIGIN_QUEUES)

/*
 * shuffle_flush_relayqs: flush relay qs (wrap for shuffle_flush_qs)
 *
 * @param sh shuffle service handle
 * @return status
 */
#define shuffle_flush_relayqs(S) \
        shuffle_flush_qs((S), SHUFFLE_RELAY_QUEUES)


/*
 * shuffle_flush_remoteqs: flush remoteqs (wrapper for shuffle_flush_qs)
 *
 * @param sh shuffle service handle
 * @return status
 */
#define shuffle_flush_remoteqs(S) \
        shuffle_flush_qs((S), SHUFFLE_REMOTE_QUEUES)


/*
 * shuffle_shutdown: drop ref to progress threads, release memory.
 * does not shutdown mercury (since we didn't start it).
 *
 * @param sh shuffle service handle
 * @return status
 */
hg_return_t shuffle_shutdown(shuffle_t sh);


/*
 * functions below here are for debugging and stat collection and
 * are not required to run the shuffle
 */

/*
 * shuffle_cfglog: setup logging before starting shuffle (for
 * debugging).  call this before shuffle_init() so that everything
 * can be properly logged... priority strings are: EMERG, ALERT, CRIT,
 * ERR, WARN, NOTE, INFO, DBG, DBG0, DBG1, DBG2, DBG3.
 * masks take the form: [facility1=]priority1,[facility2]=priority2,...
 * facilities: CLNT (client), DLIV (delivery),  SHUF (general shuffle)
 *
 * @param max_xtra_rank ranks <= this get extra logging
 * @param defpri default log priority
 * @param stderrpri if msg is logged: print to stderr if at this priority
 * @param mask log mask for non-xtra ranks
 * @param xmask log mask for xtra ranks (default=use mask)
 * @param logfile file to log to (we append the rank# to filename)
 * @param alllogs if logfile, do on all ranks (not just xtra ones)
 * @param msgbufsz size of in-memory message buffer, 0 disables
 * @param stderrlog always print log msgs to stderr, ignore stderrmask
 * @param xtra_stderlog as above, for extra ranks
 * @return 0 on success, -1 on error
 */
int shuffle_cfglog(int max_xtra_rank, const char *defpri,
                   const char *stderrpri, const char *mask,
                   const char *xmask, const char *logfile,
                   int alllogs, int msgbufsz, int stderrlog,
                   int xtra_stderrlog);

/*
 * shuffle_send_stats: retrieve shuffle sender statistics
 * @param sh shuffle service handle
 * @param local_origin accumulated number of local origin sends
 * @param local_relay accumulated number of local relay sends
 * @param remote accumulated number of remote sends
 * @return status
 */
hg_return_t shuffle_send_stats(shuffle_t sh, hg_uint64_t* local_origin,
                               hg_uint64_t* local_relay, hg_uint64_t* remote);

/*
 * shuffle_recv_stats: retrieve shuffle receiver statistics
 * @param sh shuffle service handle
 * @param rrpcs accumulated number of remote receives
 * @param lrpcs accumulated number of local receives
 * @return status
 */
hg_return_t shuffle_recv_stats(shuffle_t sh, hg_uint64_t* local,
                               hg_uint64_t* remote);


/*
 * shuffle_statedump: dump out the current state of the shuffle
 * for diagnostics
 *
 * @param sh shuffle service handle
 * @param tostderr make sure it goes to stderr too
 */
void shuffle_statedump(shuffle_t sh, int tostderr);

#ifdef __cplusplus
}
#endif
