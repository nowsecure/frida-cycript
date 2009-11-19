#!/usr/bin/env bash
PKG_ARCH=iphoneos-arm /apl/tel/exec.sh :apr-lib:libffi:readline make -f iPhone.mk "$@"
