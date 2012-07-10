#!/bin/bash

PACKAGENAME=gnuais
VERSION=0.2.4
rm -rf createdeb/
mkdir createdeb
mkdir createdeb/$PACKAGENAME-$VERSION
git archive master | tar -x -C createdeb/$PACKAGENAME-$VERSION
cd createdeb/$PACKAGENAME-$VERSION
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .
dh_make --createorig
debuild
