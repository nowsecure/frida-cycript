#!/bin/bash

set -e

for arch in armv6 armv7; do
    rm -rf "libffi.${arch}"
    mkdir "libffi.${arch}"
    cd "libffi.${arch}"
    CC="clang -arch ${arch}" CFLAGS="-no-integrated-as -isysroot $(xcodebuild -sdk iphoneos5.1 -version Path) -miphoneos-version-min=2.0" ../libffi/configure --host=arm-apple-darwin10
    make
    cd ..
done
