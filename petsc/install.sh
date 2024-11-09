#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran python3 libmpich-dev liblapack-dev

./configure --prefix=$PWD/../install/petsc_install \
	    --with-scalar-type=complex \
	    --with-debugging=1 \
	    --with-x=0
make -j4 
make PETSC_DIR=$PWD PETSC_ARCH=arch-linux-c-debug all
make PETSC_DIR=$PWD PETSC_ARCH=arch-linux-c-debug install
make PETSC_DIR=$PWD/../install/petsc_install PETSC_ARCH="" check
