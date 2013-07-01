#!/bin/bash

set -e

version=6.2
readline=readline-${version}

if [[ ! -d "${readline}" ]]; then
    ./readline.sh "${version}"
fi

exec "${0%%/*}/configure" CPPFLAGS="-I${readline}" LDFLAGS="-L${readline}" "$@"
