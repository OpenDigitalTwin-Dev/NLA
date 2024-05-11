#!/bin/sh

sudo apt update
sudo apt -y install cmake make gcc g++ gfortran libmpfr-dev libgmp-dev 

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../../install/suitesparse_install -DBLAS_openblas_LIBRARY=$PWD/../../install/openblas_install/lib/libopenblas.a -DLAPACK_openblas_LIBRARY=$PWD/../../install/openblas_install/lib/libopenblas.a
make -j4
make install
