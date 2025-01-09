#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran python3 libmpich-dev liblapack-dev libhdf5-openmpi-dev libopenmpi-dev

./configure --prefix=$PWD/../install/petsc_install \
	    --with-fftw=1 \
	    --with-fftw-dir=$PWD/../../install/fftw_install/ \
	    --with-hdf5=1 \
	    --with-hdf5-dir=/usr/lib/x86_64-linux-gnu/hdf5/openmpi/ \
	    --with-hdf5-fortran-bindings \
	    --with-zlib=1 \
	    --with-blas-lib=$PWD/../install/lapack_install/lib/libblas.a \
	    --with-lapack-lib=$PWD/../install/lapack_install/lib/liblapack.a \
	    --with-mpi=1 \
	    --with-fortran-bindings=1

make -j4 
make PETSC_DIR=$PWD PETSC_ARCH=arch-linux-c-debug all
make PETSC_DIR=$PWD PETSC_ARCH=arch-linux-c-debug install
make PETSC_DIR=$PWD/../install/petsc_install PETSC_ARCH="" check

# >>> hdf5 compiled by me can't work very strange
#	    --with-hdf5-dir=$PWD/../../install/hdf5_install/ \
# >>> for damask can't use complex
#	    --with-scalar-type=complex \
#	    --with-debugging=1 \
#	    --with-x=0 \
