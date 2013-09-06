#!/bin/bash

set -e

version=6.2
readline=readline-${version}

if [[ ! -d "${readline}" ]]; then
    ./readline.sh "${version}"; fi

if ! which aclocal; then
    touch aclocal.m4; fi
if ! which autoconf; then
    touch configure.ac; fi
if ! which automake; then
    touch Makefile.in; fi
if ! which autoheader; then
    touch config.h.in; fi

exec "${0%%/*}/configure" CPPFLAGS="-I${readline}" LDFLAGS="-L${readline}" "$@"
