#!/bin/bash
PKG_ARCH=${PKG_ARCH-iphoneos-arm} PATH=/apl/n42/pre/bin:$PATH exec /apl/tel/exec.sh :readline make "$@"
