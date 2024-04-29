#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++

mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../../install/faspsolver_install
make
make install
