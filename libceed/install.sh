#!/bin/sh

sudo apt update
sudo apt -y install cmake
sudo apt -y install make
sudo apt -y install gcc g++ gfortran liblapack-dev

make 
make prefix=$PWD/../install/libceed_install install
