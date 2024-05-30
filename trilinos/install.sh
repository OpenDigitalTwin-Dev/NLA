#!/bin/sh

apt update
apt -y install cmake
apt -y install make
apt -y install gcc g++ gfortran libmpich-dev
apt -y install p7zip-full libblas-dev liblapack-dev libboost-dev libhdf5-dev libnetcdf-dev

rm -rf build
rm -rf packages
rm -rf cmake-3.29.3-linux-x86_64/bin
7za x 1.7z -o/$PWD/packages
7za x 2.7z -o/$PWD/packages
7za x 3.7z -o/$PWD/packages
7za x 4.7z -o/$PWD/packages
7za x 5.7z -o/$PWD/packages
7za x cmake-3.29.3-linux-x86_64/bin.7z -o/$PWD/cmake-3.29.3-linux-x86_64/
7za x cmake-3.29.3-linux-x86_64/1.7z -o/$PWD/cmake-3.29.3-linux-x86_64/

mkdir build
cd build

./../cmake-3.29.3-linux-x86_64/bin/cmake \
    -DTPL_ENABLE_HDF5=ON \
    -DTrilinos_ENABLE_ALL_PACKAGES=ON \
    -DCMAKE_INSTALL_PREFIX=$PWD/../../install/trilinos_install ..

make -j8
make install
