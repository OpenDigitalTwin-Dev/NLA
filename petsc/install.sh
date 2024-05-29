#!/bin/sh

apt update
apt -y install cmake
apt -y install make
apt -y install gcc g++ gfortran python3 libmpich-dev liblapack-dev

./configure --prefix=$PWD/../install/petsc_install
make
make PETSC_DIR=$PWD PETSC_ARCH=arch-linux-c-debug install
make PETSC_DIR=$PWD/../install/petsc_install PETSC_ARCH="" check
