#!/bin/bash
. ./android.sh
rm -rf ncurses.and
mkdir ncurses.and
cd ncurses.and
cfg ../ncurses-5.9/configure --enable-static --disable-shared
sed -i -e '/^#define HAVE_LOCALE_H 1$/ d;' include/ncurses_cfg.h
make -j4
