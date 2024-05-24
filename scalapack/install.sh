#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran
sudo apt -y install libmpich-dev

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../../install/scalapack_install \
      -DBLAS_LIBRARIES=$PWD/../../install/lapack_install/lib/libblas.a \
      -DLAPACK_LIBRARIES=$PWD/../../install/lapack_install/lib/liblapack.a
make -j4
make install
