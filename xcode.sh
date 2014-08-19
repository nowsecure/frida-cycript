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

cd "${0%%/*}"

if [[ ! -e readline/libreadline.a ]]; then
    ./readline.sh; fi
if [[ ! -e libffi.a ]]; then
    ./libffi.sh; fi

if ! which aclocal; then
    touch aclocal.m4; fi
if ! which autoconf; then
    touch configure.ac; fi
if ! which automake; then
    touch Makefile.in; fi
if ! which autoheader; then
    touch config.h.in; fi

flags=("$@")

function path() {
    xcodebuild -sdk "$1" -version Path
}

xcs=$(xcode-select --print-path)
mac=$(path macosx)

function configure() {
    local dir=$1
    local sdk=$2
    local flg=$3
    shift 3

    cc=$(xcrun --sdk "${sdk}" -f clang)
    cxx=$(xcrun --sdk "${sdk}" -f clang++)
    flg+=" -isysroot $(path "${sdk}")"

    rm -rf build."${dir}"
    mkdir build."${dir}"
    cd build."${dir}"

    CC="${cc} ${flg}" CXX="${cxx} ${flg}" OBJCXX="${cxx} ${flg}" \
        ../configure --enable-maintainer-mode "${flags[@]}" --prefix="/usr" "$@"

    cd ..
}

function build() {
    local dir=$1
    local sdk=$2
    local flg=$3
    shift 3

    configure "${dir}" "${sdk}" "${flg}" "$@" --enable-static --with-pic
}

for arch in i386 x86_64; do
    build "osx-${arch}" "${mac}" "-arch ${arch} -mmacosx-version-min=10.6" \
        CPPFLAGS="-I../readline" LDFLAGS="-L../readline"
done

for arch in i386 x86_64; do
    build "sim-${arch}" iphonesimulator "-arch ${arch} -mios-simulator-version-min=4.0" \
        OBJCXXFLAGS="-fobjc-abi-version=2 -fobjc-legacy-dispatch" \
        CPPFLAGS="-I../libffi.${arch}/include" \
        LDFLAGS="-L.." \
    --disable-console
done

for arch in armv6 armv7 armv7s arm64; do
    cpf="-I../libffi.${arch}/include"
    ldf="-L.."

    flg=()
    if [[ ${arch} != armv6 ]]; then
        flg+=(--disable-console)
    else
        flg+=(LTLIBAPR="../sysroot.ios/usr/lib/libapr-1.dylib")
        flg+=(LTLIBGCC="-lgcc_s.1")
        cpf+=" -include ${PWD}/xcode.h"
        cpf+=" -mllvm -arm-reserve-r9"
        cpf+=" -I../sysroot.ios/usr/include -I../sysroot.ios/usr/include/apr-1"
        ldf+=" -L../sysroot.ios/usr/lib"
    fi

    if [[ ${arch} == arm64 ]]; then
        min=7.0
    else
        min=2.0
        #cpf+=" -mthumb"
    fi

    build "ios-${arch}" iphoneos "-arch ${arch} -miphoneos-version-min=${min}" --host=arm-apple-darwin10 \
        CPPFLAGS="${cpf}" LDFLAGS="${ldf}" "${flg[@]}" --host=arm-apple-darwin10
done
