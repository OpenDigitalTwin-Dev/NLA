#!/bin/sh

sudo apt update
sudo apt -y install cmake make gcc g++ gfortran libmpich-dev

rm -rf build
mkdir build
cd build
cmake .. \
      -DTPL_ENABLE_PARMETISLIB=ON \
      -DTPL_PARMETIS_LIBRARIES=ON \
      -DTPL_PARMETIS_INCLUDE_DIRS="$PWD/../../../install/parmetis_install/include/;$PWD/../../../NSM/extern/ALE/install/metis/include/" \
      -DTPL_PARMETIS_LIBRARIES="$PWD/../../../install/parmetis_install/lib/libparmetis.a;$PWD/../../../NSM/extern/ALE/install/metis/lib/libmetis.so" \
      -DCMAKE_INSTALL_PREFIX=$PWD/../../install/superlu_dist_install
make -j4
make install

