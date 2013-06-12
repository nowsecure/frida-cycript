#!/usr/bin/env bash

set -e

rm -rf sysroot.sim
mkdir -p sysroot.sim

xsp=$(xcode-select --print-path)
plt=iPhoneSimulator
dev=${xsp}/Platforms/${plt}.platform/Developer
sdk=${dev}/SDKs/${plt}6.1.sdk
mac=${xsp}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk

mkdir -p sysroot.sim/usr/include
cp -a "${mac}"/usr/include/ffi sysroot.sim/usr/include

mkdir -p sysroot.sim/usr/lib
cp -a /usr/lib/libffi.dylib sysroot.sim/usr/lib
cp -a /usr/lib/libapr-1.* sysroot.sim/usr/lib

ln -s /System/Library/Frameworks/WebKit.framework/Versions/A/Headers sysroot.sim/usr/include/WebKit

mkdir -p sysroot.sim/Library/Frameworks/JavaScriptCore.framework
ln -s "${sdk}"/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore sysroot.sim/Library/Frameworks/JavaScriptCore.framework
ln -s /System/Library/Frameworks/JavaScriptCore.framework/Headers sysroot.sim/Library/Frameworks/JavaScriptCore.framework

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
export OBJCXX=/usr/bin/clang++

flags=(-arch i386)
flags+=(-isysroot "${sdk}")
flags+=(-Fsysroot.sim/Library/Frameworks)

cflags=("${flags[@]}")
cflags+=(-Isysroot.sim/usr/include)
cflags+=(-fobjc-abi-version=2)
cflags+=(-Wno-overloaded-virtual)
cflags+=(-Wno-unneeded-internal-declaration)

lflags=("${flags[@]}")
lflags+=(-Lsysroot.sim/usr/lib)
lflags+=(-F"${sdk}"/System/Library/PrivateFrameworks)
lflags+=(-framework WebCore)

cflags=${cflags[*]}
export CFLAGS=${cflags}
export CXXFLAGS=${cflags}
export OBJCXXFLAGS=${cflags}

export OBJCXXFLAGS="${OBJCXXFLAGS} -fobjc-legacy-dispatch"

lflags=${lflags[*]}
export LDFLAGS=${lflags}

tflags=()
for flag in "${flags[@]}"; do
    tflags+=("-Xcompiler ${flag}")
done

tflags=${tflags[*]}
export LTFLAGS=${tflags}

export DYLD_ROOT_PATH=${sdk}
export DYLD_FALLBACK_LIBRARY_PATH=/usr/lib
export DYLD_FALLBACK_FRAMEWORK_PATH=/System/Library/Frameworks

./configure "$@"
