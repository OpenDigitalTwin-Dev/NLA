#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran libmpich-dev
sudo apt -y install p7zip-full libblas-dev liblapack-dev libboost-dev libhdf5-dev libnetcdf-dev libhdf5-openmpi-dev

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
    -DTrilinos_ENABLE_EXPLICIT_INSTANTIATION=ON \
    -DCMAKE_INSTALL_PREFIX=$PWD/../../install/trilinos_install \
    -DTPL_ENABLE_MPI=ON \
    -DTPL_ENABLE_BLAS=ON \
    -DTPL_ENABLE_LAPACK=ON \
    -DTPL_ENABLE_Boost=ON \
    -DTrilinos_EXTRA_REPOSITORIES="DataTransferKit" \
    -DTrilinos_ENABLE_DataTransferKit=ON \
    -DDataTransferKit_ENABLE_DBC=ON \
    -DDataTransferKit_ENABLE_EXPLICIT_INSTANTIATION=OFF \
    -DDataTransferKit_ENABLE_TESTS=ON \
    -DDataTransferKit_ENABLE_EXAMPLES=ON ..

make -j8
make install
