#!/bin/sh

sudo apt update
sudo apt -y install cmake make gcc g++ gfortran libmpich-dev

cd src
./configure --prefix=$PWD/../../install/hypre_install/ 
make install
