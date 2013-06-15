#!/usr/bin/env bash

set -e

rm -rf sysroot.ios
mkdir -p sysroot.ios

for deb in apr-lib_1.3.3-2 libffi_1:3.0.10-5 ncurses_5.7-12 readline_6.0-7; do
    deb=${deb}_iphoneos-arm.deb
    [[ -f "${deb}" ]] || wget http://apt.saurik.com/debs/"${deb}"
    tar=data.tar.lzma
    ar -x "${deb}" "${tar}"
    PATH=/sw/bin:$PATH tar -C sysroot.ios -xf "${tar}"
    rm -f "${tar}"
done

mkdir -p sysroot.ios/usr/include/ffi
mv -v sysroot.ios/usr/include/{,ffi/}ffi.h

plt=iPhoneOS
dev=/Developer/Platforms/${plt}.platform/Developer
sdk=${dev}/SDKs/${plt}5.0.sdk

ln -s /System/Library/Frameworks/WebKit.framework/Versions/A/Headers sysroot.ios/usr/include/WebKit

mkdir -p sysroot.ios/Library/Frameworks/JavaScriptCore.framework
ln -s "${sdk}"/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore sysroot.ios/Library/Frameworks/JavaScriptCore.framework
ln -s /System/Library/Frameworks/JavaScriptCore.framework/Headers sysroot.ios/Library/Frameworks/JavaScriptCore.framework

export CC=${dev}/usr/bin/gcc
export CXX=${dev}/usr/bin/g++
export OBJCXX=${dev}/usr/bin/g++

flags_armv6=()
flags_armv6+=(-isysroot "${sdk}")

flags_armv6+=(-Fsysroot.ios/Library/Frameworks)
flags_armv6+=(-Isysroot.ios/usr/include)
flags_armv6+=(-Lsysroot.ios/usr/lib)

flags_armv6+=(-F"${sdk}"/System/Library/PrivateFrameworks)
flags_armv6+=(-framework WebCore)


flags=(-O2)
for flag in "${flags_armv6[@]}"; do
    flags+=(-Xarch_armv6 "${flag}")
done

cflags=${flags[*]}
export CFLAGS=${cflags}
export CXXFLAGS=${cflags}
export OBJCXXFLAGS=${cflags}

lflags=()
for flag in "${flags[@]}"; do
    lflags+=("-Xcompiler ${flag}")
done

lflags=${lflags[*]}
export LTFLAGS=${lflags}

./configure --prefix=/usr "$@"
sed --in-place='' -e 's/\(-arch armv6\) -arch i386 -arch x86_64/\1/' GNUmakefile

make clean
make -j ldid=ldid all
PATH=/sw/bin:$PATH make arch=iphoneos-arm dll=dylib depends='apr-lib, readline, libffi (>= 1:3.0.10-5), adv-cmds' package
