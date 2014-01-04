#!/bin/bash

PACKAGENAME=gnuais
VERSION=0.3.0
rm -rf createdeb/
mkdir createdeb
mkdir createdeb/$PACKAGENAME-$VERSION
git archive master | tar -x -C createdeb/$PACKAGENAME-$VERSION
cd createdeb
tar -czvf $PACKAGENAME\_$VERSION.orig.tar.gz $PACKAGENAME-$VERSION
cp -r -v ../debian $PACKAGENAME-$VERSION/
cd $PACKAGENAME-$VERSION
dpkg-buildpackage
#cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .
#debuild
