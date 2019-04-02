#pragma once

#include <stddef.h>

#include <deltafs-nexus/deltafs-nexus_api.h>
#include <mercury_types.h>

#include "../src/shuffler/shuffler.h"

typedef struct xn_stat {
  struct {
    hg_uint64_t recvs; /* total rpcs received */
    hg_uint64_t sends; /* total rpcs sent */
  } local;
  struct {
    hg_uint64_t recvs; /* total rpcs received */
    hg_uint64_t sends; /* total rpcs sent */
  } remote;
} xn_stat_t;

typedef void (*hashfunc)();


/* shuffle context for the multi-hop shuffler */
typedef struct xn_ctx {
  /* replace all local barriers with global barriers */
  int force_global_barrier;
  xn_stat_t last_stat;
  xn_stat_t stat;
  nexus_ctx_t nx; /* nexus handle */
  shuffler_t sh;
  hashfunc hf; /* Pointer to hash function passed by application */
} xn_ctx_t;



/*
 * shuffle_init: initialize the shuffle service or die.
 */
void shuffle_init(xn_ctx_t* ctx, hashfunc hash_function);

/*
 * shuffle_finalize: shutdown the shuffle service and release resources.
 */
void shuffle_finalize(xn_ctx_t* ctx);
