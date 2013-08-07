#!/bin/bash

PACKAGENAME=gnuais
VERSION=0.2.8
rm -rf createdeb/
mkdir createdeb
mkdir createdeb/$PACKAGENAME-$VERSION
git archive master | tar -x -C createdeb/$PACKAGENAME-$VERSION
cp -r -v debian createdeb/$PACKAGENAME-$VERSION/
cd createdeb/$PACKAGENAME-$VERSION
dh_make --createorig -s
dpkg-buildpackage -us -uc
#cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .
#debuild -i -us -uc -b
