**All-to-all messaging via three hops across a set of distributed application processes.**

[![License](https://img.shields.io/badge/license-New%20BSD-blue.svg)](LICENSE)

DeltaFS Shuffle
===============

```
XXXXXXXXX
XX      XX                 XX                  XXXXXXXXXXX
XX       XX                XX                  XX
XX        XX               XX                  XX
XX         XX              XX   XX             XX
XX          XX             XX   XX             XXXXXXXXX
XX           XX  XXXXXXX   XX XXXXXXXXXXXXXXX  XX         XX
XX          XX  XX     XX  XX   XX       XX XX XX      XX
XX         XX  XX       XX XX   XX      XX  XX XX    XX
XX        XX   XXXXXXXXXX  XX   XX     XX   XX XX    XXXXXXXX
XX       XX    XX          XX   XX    XX    XX XX           XX
XX      XX      XX      XX XX   XX X    XX  XX XX         XX
XXXXXXXXX        XXXXXXX   XX    XX        XX  XX      XX
```

DeltaFS was developed, in part, under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S. Department of Energy/National Nuclear Security Administration. Please see the accompanying [LICENSE.txt](LICENSE.txt) for further information.

# Features

* Configurable message write-back buffering and asynchronous message sending
* Three-hop message routing instead of direct end-to-end routing

# Documentation

The shuffle uses mercury RPCs to send a message from a SRC process
to a DST process.  Our goal is to reduce per-process memory usage by
reducing the number of output queues and connections to manage on
each node (we are assuming an environment where each node has
multiple CPUs and there is an app with processes running on each
CPU).

## 3 hop overview

As an example, consider LANL Trinity Haswell nodes: there are 10,000 nodes
with 32 cores on each node.   An application running on these nodes
would have 320,000 processes (10,000 nodes * 32 cores/node).  If
all processes communicate with each other, each process will need
memory for ~320,000 output queues!

To reduce the memory usage, we add layers of indirection to the system
(in the form of additional mercury RPC hops).   The result is a 3
hop shuffle:

```
 SRC  ---na+sm--->   SRCREP ---network--->   DSTREP   ---na+sm--->  DST
           1                      2                        3

note: "na+sm" is mercury's shared memory transport, "REP" == representative
```

We divide the job for sending to each remote node among the local
cores.  Furthermore, we only send to one process on each remote node.
We expect the remote receiving process to forward our message to
the final destination (if it isn't the final destination).

Thus, on Trinity, each SRC process has 31 na+sm output queues to
talk to other local processes, and each SRC process has 10,000/32
(~313) network output queues to talk to the remote nodes it is
responsible for.   This is much less than the ~320,000 output queues
needed in the all-to-all case.

A msg from a SRC to a remote DST on node N flows like this:
1. SRC find the local proc responsible for talking to N.  this is
   the SRCREP.   it forward the msg to the SRCREP over na+sm.
2. the SRCREP forwards all messages for node N to one process on
   node N over the network.   this is the DSTREP.
3. the DSTREP receives the message and looks for its na+sm connection
   to the DST (which is also on node N) and sends the msg to DST.
at that point the DST will deliver the msg.   Note that it is
possible to skip hops (e.g. if SRC==SRCREP, the first hop can be
skipped).

The shuffle library manages this three hop communication.  It
has support for batching multiple messages into a larger batch
message (to reduce the overhead of small writes), and it also
supports write-behind buffering (so that the application can
queue data in the buffer and continue, rather than waiting for
the RPC).  If/when the buffers fill, the library provides flow
control to stop the application from overfilling the buffers.

For output queues we provide:
* maxrpc:    the max number of outstanding RPCs we allow at one time.
             additional requests are queued on a wait list until
             something completes.
* buftarget: controls batching of requests... we batch multiple
             requests into a single RPC until we have at least
             "buftarget" bytes in the batch.  setting this value
             small effectively disables batching.

Also for delivery, we have a "deliverq_max" which is the max
number of delivery requests we will buffer before we start
putting additional requests on the waitq (waitq requests are
not ack'd until space is available... this triggers flow control).

Note that we identify endpoints by a global rank number (the
rank number is assigned by MPI... MPI is also used to determine
the topology -- i.e. which ranks are on the local node.  See
deltafs-nexus for details...).

## shuffle API

The shuffle API is a C-style API that operates on shuffle handles
objects (shuffle_t).  To allocate and init a shuffle object, you
first must have init'd mercury and allocated a nexus context
for routing (e.g. with nexus_bootstrap() -- see the deltafs-nexus
repository for details on the nexus API).   One that is done,
you can call shuffle_init() to allocate a shuffle object.
As part of the shuffle_init() call, you must provide a pointer to a
callback function that is called to deliver shuffle data that has
been sent to you by other processes.   To queue data for sending
through the shuffle service, use the shuffle_enqueue() call.  The
shuffle service provides a set of calls that can be used to flush
out buffered data.  When the application is complete, the shuffle_shutdown()
should be used to retire the shuffle handle.

The prototype for shuffle_init() is:
```
#include <deltafs-shuffle/shuffle_api.h>

/*
 * shuffle_init: inits the shuffle layer.
 */
shuffle_t shuffle_init(nexus_ctx_t nxp, char *funname,
          shuffle_deliverfn_t delivercb, struct shuffle_opts *sopt);

```

The nexus context provided is used to provide 3 hop routing.
The "funname" is a string used to register the shuffle RPC with
mercury (having this as a argument allows applications to have
more than one instance of a shuffle active at the same time).

The deliver callback is called when shuffle data is received:
```
typedef void (*shuffle_deliverfn_t)(int src, int dst, uint32_t type,
                                    void *d, uint32_t datalen);
```
The deliver callback function is allowed to block, as it is called
in the context of the shuffle's delivery thread.   A blocked delivery
thread creates backpressure and will eventually trigger the shuffle's
flow control mechanisms.

The shuffle_opts structure contains all the shuffle's flow control
and batching/queuing options:
```
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
```

To init the shuffle_opts to the default values, use shuffle_opts_init():
```
void shuffle_opts_init(struct shuffle_opts *sopt);
```

To enqueue data for sending over the shuffle service, use the
shuffle_enqueue() API:
```
hg_return_t shuffle_enqueue(shuffle_t sh, int dst, uint32_t type,
                            void *d, uint32_t datalen);
```

The "dst" is the global rank of the destination process.   The "type"
is a per-message int that the shuffle service makes available to users
to use in an application-dependent way -- it is passed through the
shuffle layer to the delivery callback function.

The shuffle services provides 4 flush functions.  These functions
operate only on the local queues.  They can be combined with collective
ops (e.g. MPI_Barrier()) to build higher-level flush operations.

The delivery flush function blocks until all requests currently
in the delivery queue are delivered.   It makes no claims about
requests that arrive after the flush has started.
```
hg_return_t shuffle_flush_delivery(shuffle_t sh);
```

The other three flush functions flush the specified output queues.
These function blocks until all requests currently in the specified
output queues are delivered.  Shuffle makes no claims about requests that
arrive after the flush has been started.
```
hg_return_t shuffle_flush_originqs(shuffle_t sh);
hg_return_t shuffle_flush_relayqs(shuffle_t sh);
hg_return_t shuffle_flush_remoteqs(shuffle_t sh);
```

To shut down the shuffle service use shuffle_shutdown():
```
hg_return_t shuffle_shutdown(shuffle_t sh);
```

In addition to the above functions, the shuffle service also provides
a set of debug and stat functions.  The prototypes are shown below.
See the mercury-shuffle/shuffle_api.h header file for additional info.
```
/* setup and enable shuffle logging */
int shuffle_cfglog(int max_xtra_rank, const char *defpri,
                   const char *stderrpri, const char *mask,
                   const char *xmask, const char *logfile,
                   int alllogs, int msgbufsz, int stderrlog,
                   int xtra_stderrlog);

/* retrieve shuffle sender statistics */
hg_return_t shuffle_send_stats(shuffle_t sh, hg_uint64_t* local_origin,
                               hg_uint64_t* local_relay, hg_uint64_t* remote);

/* retrieve shuffle receiver statistics */
hg_return_t shuffle_recv_stats(shuffle_t sh, hg_uint64_t* local,
                               hg_uint64_t* remote);


/* dump out the current state of the shuffle for diagnostics */
void shuffle_statedump(shuffle_t sh, int tostderr);
```

## nexus-runner program

The nexus-runner program (available in its own git repository)
is an MPI-based shuffle test/demo program.  It should be launched
across a set of one or more nodes using MPI (e.g. mpirun).  Each
process will init the nexus routing library and the shuffle code.
Processes will then send the requested number of messages through
the 3 hop shuffle, flush the system, and then exit.

# Software requirements

First, if on a Ubuntu box, do the following:

```bash
sudo apt-get install -y gcc g++ make
sudo apt-get install -y pkg-config
sudo apt-get install -y autoconf automake libtool
sudo apt-get install -y cmake
sudo apt-get install -y libmpich-dev
sudo apt-get install -y mpich
```

If on a Mac machine with `brew`, do the following:

```bash
brew install gcc
brew install pkg-config
brew install autoconf automake libtool
brew install cmake
env CC=/usr/local/bin/gcc-<n> CXX=/usr/local/bin/g++-<n> \
  brew install --build-from-source mpich
```

## Mercury

Next, we need to install `mercury`. This can be done as follows:

```bash
git clone git@github.com:mercury-hpc/mercury.git
cd mercury
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=</tmp/mercury-prefix> \
  -DNA_USE_SM=ON \
..
```

## DeltaFS Nexus

Last, we need to install `deltafs-nexus`. This can be done as follows:

```bash
git clone git@github.com:pdlfs/deltafs-nexus.git
cd deltafs-nexus
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=</tmp/deltafs-nexus-prefix> \
  -DCMAKE_PREFIX_PATH=</tmp/mercury-prefix> \
..
```

# Building

After installing dependencies, we can build `deltafs-shuffle` as follows:

```bash
cd deltafs-shuffle
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="</tmp/deltafs-nexus-prefix>;</tmp/mercury-prefix>" \
  -DCMAKE_INSTALL_PREFIX=</tmp/deltafs-shuffle-prefix> \
..
```
