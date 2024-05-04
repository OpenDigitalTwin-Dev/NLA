#!/bin/sh

sudo apt update
sudo apt -y install cmake make gcc g++ gfortran libmpich-dev liblapack-dev libscotch-dev libmetis-dev libscalapack-openmpi-dev

cp Make.inc/Makefile.debian.PAR ./Makefile.inc
make all -j4
mkdir -p ../install/mumps_install/lib
mkdir ../install/mumps_install/include
cp -r lib ../install/mumps_install/
cp -r include ../install/mumps_install/
