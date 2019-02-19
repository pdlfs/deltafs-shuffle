**All-to-all messaging via three hops across a set of distributed application processes.**

[![License](https://img.shields.io/badge/license-New%20BSD-blue.svg)](LICENSE)

deltafs-shuffle
===============

```
          xxx                                 xxx
         xxx                                   xxx
        xxx                                     xxx
       xxx                                       xxx
      xxx                   xxxx                  xxx
     xxx      x           xx    xx                 xxx
    xxx      xx          xx      xx                 xxx
   xxx      xxx                 xx          x        xxx
  xxx      xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  xx   xxxxxxxx
 xxx        xxx               xxx           xxx        xxx
  xxxxxxxx   xx  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx      xxx
   xxx        x                 xx          xxx      xxx
    xxx                          xx         xx      xxx
     xxx                 xx     xx          x      xxx
      xxx                  xxxxx                  xxx
       xxx                                       xxx
        xxx                                     xxx
         xxx                                   xxx
          xxx                                 xxx
```

Please see the accompanying LICENSE file for licensing details. `deltafs-shuffle` is still under development.

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

Lastly, we need to install `deltafs-nexus`. This can be done as follows:

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
  -DCMAKE_INSTALL_PREFIX=</tmp/deltafs-shuffle-prefix> \
  -DCMAKE_PREFIX_PATH=</tmp/deltafs-nexus-prefix>;\
    </tmp/mercury-prefix> \
..
```
