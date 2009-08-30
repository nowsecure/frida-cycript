#!/bin/bash
set -e
export PKG_ARCH=${PKG_ARCH-iphoneos-arm}
PATH=/apl/n42/pre/bin:$PATH /apl/tel/exec.sh com.saurik.winterboard make "$@"
export CODESIGN_ALLOCATE=$(which arm-apple-darwin9-codesign_allocate)
/apl/tel/util/ldid -S *.dylib
make package
