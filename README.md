**All-to-all messaging across a set of distributed application processes.**

[![License](https://img.shields.io/badge/license-New%20BSD-blue.svg)](LICENSE)

deltafs-shuffle
===============

```
      xxx                                           xxx
     xxx                                             xxx
    xxx                                               xxx
   xxx                                                 xxx
  xxx      xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx       xxxx
  xxx                                                   xxx
   xxx                                                 xxx
    xxx                                               xxx
     xxx                                             xxx
      xxx                                           xxx
```

# Software requirements

First, if on a Ubuntu box, do the following:

```bash
sudo apt-get install -y gcc g++ make cmake pkg-config
sudo apt-get install -y autoconf automake libtool
sudo apt-get install -y libmpich-dev
sudo apt-get install -y mpich
```

If on a Mac machine with `brew`, do the following:

```bash
brew install gcc cmake pkg-config
brew install autoconf automake libtool
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
