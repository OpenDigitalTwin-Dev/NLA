#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../../install/superlu_install -DTPL_BLAS_LIBRARIES=$PWD/../../install/openblas_install/lib/libopenblas.a
make -j4
make install
