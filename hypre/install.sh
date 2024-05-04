#!/bin/sh

cd src
./configure --prefix=$PWD/../../install/hypre_install/ 
make install
