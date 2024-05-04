#!/bin/sh

rm -rf ../install/hypre_install
cd src
make clean
make distclean
