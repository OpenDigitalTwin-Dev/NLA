#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran python3 libmpich-dev liblapack-dev
sudo apt -y install p7zip-full

7za x samples.7z -o/$PWD/

make -j4
make PREFIX=$PWD/../install/libxsmm_install install
