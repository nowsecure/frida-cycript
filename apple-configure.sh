#!/usr/bin/env bash

# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2015  Jay Freeman (saurik)

# GNU Affero General Public License, Version 3 {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# }}}

set -e

cd "${0%%/*}"

if [[ ! -e readline.osx/libreadline.a ]]; then
    ./apple-readline.sh; fi
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

gof=(-g0 -O3)

for arch in i386 x86_64; do
    build "osx-${arch}" "${mac}" "-arch ${arch} -mmacosx-version-min=10.6" \
        CFLAGS="${gof[*]}" CXXFLAGS="${gof[*]}" OBJCXXFLAGS="${gof[*]}" \
        CPPFLAGS="-I../readline.osx" LDFLAGS="-L../readline.osx"
done

for arch in i386 x86_64; do
    build "sim-${arch}" iphonesimulator "-arch ${arch} -mios-simulator-version-min=4.0" \
        CFLAGS="${gof[*]}" CXXFLAGS="${gof[*]}" OBJCXXFLAGS="${gof[*]} -fobjc-abi-version=2 -fobjc-legacy-dispatch" \
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
        flg+=(LTLIBGCC="-lgcc_s.1")
        cpf+=" -include ${PWD}/xcode.h"
        cpf+=" -mllvm -arm-reserve-r9"
        cpf+=" -I../sysroot.ios/usr/include"
        ldf+=" -L../sysroot.ios/usr/lib"
    fi

    if [[ ${arch} == arm64 ]]; then
        min=7.0
    else
        min=2.0
        ldf+=" -Wl,-segalign,4000"
        #cpf+=" -mthumb"
    fi

    build "ios-${arch}" iphoneos "-arch ${arch} -miphoneos-version-min=${min}" --host=arm-apple-darwin10 \
        CFLAGS="${gof[*]}" CXXFLAGS="${gof[*]}" OBJCXXFLAGS="${gof[*]}" \
        CPPFLAGS="${cpf}" LDFLAGS="${ldf}" "${flg[@]}" --host=arm-apple-darwin10
done
