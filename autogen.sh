#!/usr/bin/env bash
libtoolize -cif
aclocal -I m4
autoconf
automake -acf
