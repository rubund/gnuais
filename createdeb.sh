#!/bin/bash

PACKAGENAME=gnuais
VERSION=0.3.2
rm -rf createdeb/
mkdir createdeb
#rm -rf createdeb/$PACKAGENAME-$VERSION
mkdir createdeb/$PACKAGENAME-$VERSION
git archive master | tar -x -C createdeb/$PACKAGENAME-$VERSION
cd createdeb
#tar -xvf $PACKAGENAME\_$VERSION.orig.tar.gz
tar -czvf $PACKAGENAME\_$VERSION.orig.tar.gz $PACKAGENAME-$VERSION
cp -r -v ../debian $PACKAGENAME-$VERSION/
cd $PACKAGENAME-$VERSION
dpkg-buildpackage -S
#cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .
#debuild
