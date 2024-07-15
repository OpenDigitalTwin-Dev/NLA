#!/bin/sh

sudo apt update
sudo apt -y install cmake make gcc g++ python3
sudo apt -y install p7zip-full

7za x samples.7z -o/$PWD/

make -j4
make PREFIX=$PWD/../install/libxsmm_install install
