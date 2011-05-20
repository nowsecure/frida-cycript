#!/usr/bin/env bash

mkdir -p sysroot.ios

for deb in apr-lib_1.3.3-2 readline_6.0-7; do
    deb=${deb}_iphoneos-arm.deb
    #wget http://apt.saurik.com/debs/"${deb}"
    tar=data.tar.lzma
    ar -x "${deb}" "${tar}"
    tar -C sysroot.ios -xf "${tar}"
    rm -f "${tar}"
done

dev=/Developer/Platforms/iPhoneOS.platform/Developer
export CC=${dev}/usr/bin/gcc
export CXX=${dev}/usr/bin/g++

flags_armv6=()
flags_armv6+=(-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.3.sdk)

flags_armv6+=(-Isysroot.ios/usr/include)
flags_armv6+=(-Lsysroot.ios/usr/lib)

flags=()
for flag in "${flags_armv6[@]}"; do
    flags+=(-Xarch_armv6 "${flag}")
done

cflags=${flags[*]}
export CFLAGS=${cflags}
export CXXFLAGS=${cflags}

lflags=()
for flag in "${flags[@]}"; do
    lflags+=("-Xcompiler ${flag}")
done

lflags=${lflags[*]}
export LTFLAGS=${lflags}

./configure --prefix=/usr "$@"
