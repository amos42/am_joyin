#!/bin/bash
[ -z $1 ] && echo "usage : utils/makepackage.sh version" && exit 1
version=$1
[ ! -e "dkms.conf" ] && echo "start this script from base directory" && exit 1
rm -rf build || (echo "unable to clean build directory" && exit 1)
mkdir build || (echo "unable to create build directory" && exit 1)
mkdir build/am_joyin-$version || (echo "unable to create package directory" && exit 1)
rootdir="build/am_joyin-${version}"

echo "copying resources"
srcdir="$rootdir/usr/src/am_joyin-${version}"
sharedir="$rootdir/usr/share/am_joyin-${version}"

cp -r DEBIAN "$rootdir"

mkdir -p "$srcdir"
cp dkms.conf "$srcdir"
cp Makefile "$srcdir"
cp *.c "$srcdir"
cp *.h "$srcdir"

mkdir -p "$sharedir"
cp LICENSE "$sharedir"
cp README.md "$sharedir"

#sed -i "s/\$MKVERSION/${version}/g" $srcdir/src/* $rootdir/DEBIAN/control $rootdir/DEBIAN/prerm
sed -i "s/\$MKVERSION/${version}/g" $srcdir/Makefile $srcdir/dkms.conf $rootdir/DEBIAN/control $rootdir/DEBIAN/prerm

echo "building dpkg"

cd build
sudo dpkg-deb --build "am_joyin-${version}/"
