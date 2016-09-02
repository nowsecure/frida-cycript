#!/bin/bash
set -e
source ./android.sh
rm -rf build.and-armeabi
mkdir -p build.and-armeabi
cd build.and-armeabi
cpp+=(-I../ncurses.and/include -I../readline.and -I../android)
ldf+=(-L../ncurses.and/lib -L../readline.and -L../android/armeabi)
cfg ../configure --enable-maintainer-mode "$@"
