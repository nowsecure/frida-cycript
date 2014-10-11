#!/bin/bash
set -e
. ./android.sh
arch=armeabi
rm -rf readline.and
mkdir -p readline.and
cd readline.and
cfg ../readline/configure bash_cv_wcwidth_broken=no
make
ln -sf . readline
