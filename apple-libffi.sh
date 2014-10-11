#!/usr/bin/env bash

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
    local min=$5
    shift 5

    rm -rf "libffi.${arch}"
    if ! isysroot=$(xcodebuild -sdk "${sdk}" -version Path); then
        return
    fi

    archs+=("${arch}")
    mkdir "libffi.${arch}"

    flags=()
    flags+=(-isysroot "${isysroot}")
    flags+=(-m${os}-version-min="${min}")
    flags+=(-no-integrated-as)
    flags+=(-fno-stack-protector)
    flags+=(-O3 -g3)

    if [[ ${arch} == arm* && ${arch} != arm64 ]]; then
        flags+=(-mthumb)
    fi

    cd "libffi.${arch}"
    CC="clang -arch ${arch}" CFLAGS="${flags[*]}" CPPFLAGS="${flags[*]} $*" ../libffi/configure --host="${host}"
    make
    cd ..
}

arch armv6 arm-apple-darwin10 iphoneos iphoneos 2.0 -mllvm -arm-reserve-r9
arch armv7 arm-apple-darwin10 iphoneos iphoneos 2.0
arch armv7s arm-apple-darwin10 iphoneos iphoneos 2.0
arch arm64 aarch64-apple-darwin11 iphoneos iphoneos 2.0

arch i386 i386-apple-darwin10 iphonesimulator ios-simulator 4.0
arch x86_64 x86_64-apple-darwin11 iphonesimulator ios-simulator 4.0

libffi=()
for arch in "${archs[@]}"; do
    a=libffi."${arch}"/.libs/libffi.a
    # sectionForAddress(...) address not in any section file '...' for architecture i386
    ar m "${a}" src/prep_cif.o
    libffi+=("${a}")
done

lipo -create -output libffi.a "${libffi[@]}"
