#!/bin/sh

apt update
apt -y install cmake
apt -y install make
apt -y install gcc g++ gfortran

make 
make prefix=$PWD/../install/libceed_install install
