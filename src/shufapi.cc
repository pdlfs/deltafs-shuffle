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

#include "deltafs-shuffle/shufapi.h"
#include "shuffler/shuffler.h"

#ifdef __cplusplus
extern "C" {
#endif

static char deltafs_shuf_id[] = "deltafs_shuf_rpc";

struct deltafs_shuf {
  nexus_ctx_t nxh;
  bool owns_nxh;
  shuffler_t sh;
};

struct deltafs_shuf* deltafs_shuf_init(const struct deltafs_shuf_opts* opts) {
  struct deltafs_shuf* is = NULL;
  bool owns_nxh = false;
  nexus_ctx_t nxh;

  if (opts->nxh) {
    nxh = static_cast<nexus_ctx_t>(opts->nxh);
  } else {
    nxh = nexus_bootstrap(opts->subnet, opts->proto);
    owns_nxh = true;
  }

  shuffler_t sh = shuffler_init(
      nxh, deltafs_shuf_id, opts->lsenderlimit, opts->rsenderlimit,
      opts->lomaxrpc, opts->lobuftarget, opts->lrmaxrpc, opts->lrbuftarget,
      opts->rmaxrpc, opts->rbuftarget, opts->deliverq_max, opts->deliverq_min,
      opts->hdlr);

  if (sh) {
    is = static_cast<struct deltafs_shuf*>(malloc(sizeof(struct deltafs_shuf)));
    is->nxh = nxh;
    is->owns_nxh = owns_nxh;
    is->sh = sh;
  } else if (owns_nxh) {
    nexus_destroy(nxh);
  }

  return is;
}

int deltafs_shuf_localbarrier(struct deltafs_shuf* is) {
  return nexus_local_barrier(is->nxh) == NX_SUCCESS ? 0 : -1;
}

int deltafs_shuf_localcommsz(struct deltafs_shuf* is) {
  return nexus_local_size(is->nxh);
}

int deltafs_shuf_localid(struct deltafs_shuf* is) {
  return nexus_local_rank(is->nxh);
}

int deltafs_shuf_barrier(struct deltafs_shuf* is) {
  return nexus_global_barrier(is->nxh) == NX_SUCCESS ? 0 : -1;
}

int deltafs_shuf_commsz(struct deltafs_shuf* is) {
  return nexus_global_size(is->nxh);
}

int deltafs_shuf_myid(struct deltafs_shuf* is) {
  return nexus_global_rank(is->nxh);
}

hg_return_t deltafs_shuf_enqueue(struct deltafs_shuf* is, int dst, void* msg,
                                 uint32_t msglen) {
  return shuffler_send(is->sh, dst, 0, msg, msglen);
}

hg_return_t deltafs_shuf_flushoriqs(struct deltafs_shuf* is) {
  return shuffler_flush_originqs(is->sh);
}

hg_return_t deltafs_shuf_flushremoteqs(struct deltafs_shuf* is) {
  return shuffler_flush_remoteqs(is->sh);
}

hg_return_t deltafs_shuf_flushrelayqs(struct deltafs_shuf* is) {
  return shuffler_flush_relayqs(is->sh);
}

hg_return_t deltafs_shuf_flushdeliv(struct deltafs_shuf* is) {
  return shuffler_flush_delivery(is->sh);
}

void deltafs_shuf_free(struct deltafs_shuf* is) {
  shuffler_shutdown(is->sh);
  if (is->owns_nxh) {
    nexus_destroy(is->nxh);
  }
  free(is);
}

#ifdef __cplusplus
}
#endif
