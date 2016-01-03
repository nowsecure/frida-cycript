#!/bin/bash
# XXX: this script is required because, despite claiming clang is so awesome due to having libclang, Google doesn't ship a usable version
xcs=$(xcode-select --print-path)
xct="${xcs}/Toolchains/XcodeDefault.xctoolchain/usr/lib"
./android-configure.sh --with-libclang="-rpath ${xct} ${xct}/libclang.dylib"
