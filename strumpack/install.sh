#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../../install/strumpack_install \
      -DTPL_BLAS_LIBRARIES=$PWD/../../install/lapack_install/lib/libblas.a \
      -DTPL_LAPACK_LIBRARIES=$PWD/../../install/lapack_install/lib/liblapack.a \
      -DTPL_SCALAPACK_LIBRARIES=$PWD/../../install/scalapack_install/lib/libscalapack.a
make -j4
make install
