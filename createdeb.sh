#!/bin/bash

PACKAGENAME=gnuais
VERSION=0.2.6
rm -rf createdeb/
mkdir createdeb
mkdir createdeb/$PACKAGENAME-$VERSION
git archive ubuntu | tar -x -C createdeb/$PACKAGENAME-$VERSION
cd createdeb/$PACKAGENAME-$VERSION
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr .
dh_make --createorig -s
debuild -i -us -uc -b
