#!/usr/bin/env bash
set -e
aclocal
automake -acf
libtoolize -ci
autoconf
