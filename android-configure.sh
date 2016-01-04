#!/bin/bash
set -e
source ./android.sh
rm -rf build.and-armeabi
mkdir -p build.and-armeabi
cd build.and-armeabi
cpp+=(-I../ncurses.and/include -I../readline.and -I../android -I../libffi.and/include)
ldf+=(-L../ncurses.and/lib -L../readline.and -L../android/armeabi -L../libffi.and/.libs)
cfg ../configure --enable-maintainer-mode "$@"
