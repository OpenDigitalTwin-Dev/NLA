#!/bin/sh

# need to install petsc first

export PETSC_DIR=$PWD/../petsc
export PETSC_ARCH=arch-linux-c-debug
./configure --prefix=$PWD/../install/slepc_install
make SLEPC_DIR=$PWD PETSC_DIR=$PWD/../petsc PETSC_ARCH=arch-linux-c-debug
make SLEPC_DIR=$PWD PETSC_DIR=$PWD/../petsc install
make SLEPC_DIR=$PWD PETSC_DIR=$PWD/../petsc PETSC_ARCH=arch-linux-c-debug check
