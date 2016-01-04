#!/usr/bin/env bash

# Cycript - The Truly Universal Scripting Language
# Copyright (C) 2009-2016  Jay Freeman (saurik)

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

flags=("$@")
ccf=(-g0 -O3)

function path() {
    xcodebuild -sdk "$1" -version Path
}

xcs=$(xcode-select --print-path)
mac=$(path macosx)
xct="${xcs}/Toolchains/XcodeDefault.xctoolchain/usr/lib"

system=1

function configure() {
    local dir=$1
    local sdk=$2
    local arc=$3
    local min=$4
    local ffi=$5
    local cpf=$6
    local ldf=$7
    local obc=$8
    shift 8

    set -- "$@" --enable-static --with-pic

    cc=$(xcrun --sdk "${sdk}" -f clang)
    cxx=$(xcrun --sdk "${sdk}" -f clang++)

    flg="-arch ${arc} ${min}"
    flg+=" -isysroot $(path "${sdk}")"

    rm -rf build."${dir}"
    mkdir build."${dir}"
    cd build."${dir}"

    if "${ffi}"; then
        cpf+=" -I../libffi.${arch}/include"
        ldf+=" -L../libffi.${arch}/.libs"
    fi

    cpf+=" -I../libuv/include"
    ldf+=" -L../libuv.${arch}/.libs"

    ../configure --enable-maintainer-mode "${flags[@]}" --prefix="/usr" "$@" \
        --with-libclang="-rpath ${xct} ${xct}/libclang.dylib" \
        CC="${cc} ${flg}" CXX="${cxx} ${flg}" OBJCXX="${cxx} ${flg}" \
        CFLAGS="${ccf[*]}" CXXFLAGS="${ccf[*]}" OBJCXXFLAGS="${ccf[*]} ${obc}" \
        CPPFLAGS="${cpf}" LDFLAGS="${ldf}" CY_SYSTEM="$((1<<system++))"

    cd ..
}

for arch in i386 x86_64; do
    configure "osx-${arch}" "${mac}" "${arch}" "-mmacosx-version-min=10.6" \
        false "-I../readline.osx" "-L../readline.osx" "" \
        --with-python=/usr/bin/python-config
done

for arch in i386 x86_64; do
    configure "sim-${arch}" iphonesimulator "${arch}" "-mios-simulator-version-min=4.0" \
        true "" "" "-fobjc-abi-version=2 -fobjc-legacy-dispatch" \
        --disable-console
done

for arch in armv6 armv7 armv7s arm64; do
    cpf=""
    ldf=""

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

    configure "ios-${arch}" iphoneos "${arch}" "-miphoneos-version-min=${min}" \
        true "${cpf[*]}" "${ldf[*]}" "" \
        --host=arm-apple-darwin10 LFLAGS="--ecs --meta-ecs" "${flg[@]}"
done
