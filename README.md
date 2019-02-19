**All-to-all messaging across a set of distributed application processes.**

[![License](https://img.shields.io/badge/license-New%20BSD-blue.svg)](LICENSE)

deltafs-shuffle
===============

# Software requirements

First, if on a Ubuntu box, do the following:

```bash
sudo apt-get install -y gcc g++ make cmake pkg-config
sudo apt-get install -y autoconf automake libtool
sudo apt-get install -y libmpich-dev
sudo apt-get install -y mpich
```

On a Mac machine with `brew`, do the following:

```bash
brew install gcc cmake pkg-config
brew install autoconf automake libtool
env CC=/usr/local/bin/gcc-<n> CXX=/usr/local/bin/g++-<n> \
  brew install --build-from-source mpich
```

Next, we need to install `mercury` and `deltafs-nexus`. This can be done as follows:

# Building

After installing dependencies, we can build `deltafs-shuffle` as follows:

```bash
cd deltafs-shuffle
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/tmp/deltafs-shuffle \
  -DCMAKE_PREFIX_PATH= \
  ..
```
