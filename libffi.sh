#!/bin/bash

# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2013  Jay Freeman (saurik)

# GNU General Public License, Version 3 {{{
#
# Cycript is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# Cycript is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
# }}}

set -e

archs=()
function arch() {
    local arch=$1
    local host=$2
    local sdk=$3
    local os=$4
    shift 4

    rm -rf "libffi.${arch}"
    if ! isysroot=$(xcodebuild -sdk "${sdk}" -version Path); then
        return
    fi

    archs+=("${arch}")
    mkdir "libffi.${arch}"

    cd "libffi.${arch}"
    CC="clang -arch ${arch}" CFLAGS="-no-integrated-as -isysroot ${isysroot} -m${os}-version-min=2.0" ../libffi/configure --host="${host}"
    make
    cd ..
}

arch armv6 arm-apple-darwin10 iphoneos5.1 iphoneos
arch armv7 arm-apple-darwin10 iphoneos iphoneos
arch armv7s arm-apple-darwin10 iphoneos iphoneos
arch i386 i386-apple-darwin10 iphonesimulator ios-simulator

libffi=()
for arch in "${archs[@]}"; do
    libffi+=(libffi."${arch}"/.libs/libffi.a)
done

lipo -create -output libffi.a "${libffi[@]}"
