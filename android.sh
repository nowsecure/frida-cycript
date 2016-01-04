#!/bin/bash
ndk=~/bin/android-ndk
abi=armeabi
ver=4.9
api=9
bld=darwin-x86_64
bin=${ndk}/toolchains/arm-linux-androideabi-${ver}/prebuilt/${bld}/bin
#export PATH=${bin}:$PATH
flg=()
flg+=(--sysroot=${ndk}/platforms/android-${api}/arch-arm)
flg+=(-I${ndk}/sources/cxx-stl/gnu-libstdc++/${ver}/include)
flg+=(-I${ndk}/sources/cxx-stl/gnu-libstdc++/${ver}/libs/${abi}/include)
ldf=()
ldf+=(-L${ndk}/sources/cxx-stl/gnu-libstdc++/${ver}/libs/${abi})
ldf+=(-lgnustl_static)
tgt=arm-linux-androideabi
cc=${bin}/${tgt}-gcc
cxx=${bin}/${tgt}-g++
cpp=()
ccf=()
ccf+=(-g0 -O3)
function cfg() {
    cfg=$1
    shift
    "${cfg}" \
        CC="${cc} ${flg[*]}" CXX="${cxx} ${flg[*]}" OBJCXX="${cxx} ${flg[*]}" \
        CFLAGS="${ccf[*]}" CXXFLAGS="${ccf[*]}" OBJCXXFLAGS="${ccf[*]}" \
        --host="${tgt}" CPPFLAGS="${cpp[*]}" LDFLAGS="${ldf[*]}" "$@"
}
