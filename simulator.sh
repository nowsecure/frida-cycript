#!/usr/bin/env bash

set -e

rm -rf sysroot.sim
mkdir -p sysroot.sim

plt=iPhoneSimulator
dev=/Developer/Platforms/${plt}.platform/Developer
sdk=${dev}/SDKs/${plt}5.0.sdk

mkdir -p sysroot.sim/usr/include
cp -a /usr/include/ffi sysroot.sim/usr/include

mkdir -p sysroot.sim/usr/lib
cp -a /usr/lib/libffi.dylib sysroot.sim/usr/lib
cp -a /usr/lib/libapr-1.* sysroot.sim/usr/lib

ln -s /System/Library/Frameworks/WebKit.framework/Versions/A/Headers sysroot.sim/usr/include/WebKit

mkdir -p sysroot.sim/Library/Frameworks/JavaScriptCore.framework
ln -s "${sdk}"/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore sysroot.sim/Library/Frameworks/JavaScriptCore.framework
ln -s /System/Library/Frameworks/JavaScriptCore.framework/Headers sysroot.sim/Library/Frameworks/JavaScriptCore.framework

export CC=${dev}/usr/bin/gcc
export CXX=${dev}/usr/bin/g++
export OBJCXX=${dev}/usr/bin/g++

flags_i386=()
flags_i386+=(-isysroot "${sdk}")

flags_i386+=(-Fsysroot.sim/Library/Frameworks)
flags_i386+=(-Isysroot.sim/usr/include)
flags_i386+=(-Lsysroot.sim/usr/lib)

flags_i386+=(-F"${sdk}"/System/Library/PrivateFrameworks)
flags_i386+=(-framework WebCore)

flags=()
for flag in "${flags_i386[@]}"; do
    flags+=(-Xarch_i386 "${flag}")
done

flags+=(-fobjc-abi-version=2)

cflags=${flags[*]}
export CFLAGS=${cflags}
export CXXFLAGS=${cflags}
export OBJCXXFLAGS=${cflags}

export OBJCXXFLAGS="${OBJCXXFLAGS} -Xarch_i386 -fobjc-legacy-dispatch"

lflags=()
for flag in "${flags[@]}"; do
    lflags+=("-Xcompiler ${flag}")
done

lflags=${lflags[*]}
export LTFLAGS=${lflags}

./configure --prefix=/usr "$@"
