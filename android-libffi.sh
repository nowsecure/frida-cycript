#!/bin/bash
set -e
. ./android.sh
arch=armeabi
rm -rf libffi.and
mkdir -p libffi.and
cd libffi.and
cfg ../libffi/configure --enable-static --disable-shared
make
