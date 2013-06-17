#!/usr/bin/env bash

# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2013  Jay Freeman (saurik)

# GNU General Public License, Version 3 {{{
#
# Cycript is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# Cycript is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
# }}}

filters=("$@")

while IFS= read -r line; do
    if [[ ${line} = @if* ]]; then
        line=${line#@if }
        for name in "${filters[@]}"; do
            if [[ ${line} = ${name}' '* ]]; then
                echo "${line#${name} }"
            fi
        done
    elif [[ ${line} = @begin* ]]; then
        set ${line}; shift
        filter=
        for name in "${filters[@]}"; do
            for side in "$@"; do
                if [[ ${name} == ${side} ]]; then
                    unset filter
                fi
            done
        done
    elif [[ ${line} = @end ]]; then
        unset filter
    elif [[ -z ${filter+@} ]]; then
        echo "${line}"
    fi
done
