#!/usr/bin/env bash

# Cycript - The Truly Universal Scripting Language
# Copyright (C) 2009-2016  Jay Freeman (saurik)

# GNU Affero General Public License, Version 3 {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# }}}

file=$1
shift

filters=("$@")

function include() {
    file=$1
    shift

    dir=/${file}
    dir=${dir%/*}
    dir=${dir:-/.}
    dir=${dir#/}
    dir=${dir}/

    while IFS= read -r line; do
        if false; then :
        elif [[ ${line} = @if* ]]; then
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
        elif [[ ${line} = @else ]]; then
            if [[ -z ${filter+@} ]]; then
                unset filter
            else
                filter=
            fi
        elif [[ ${line} = @end ]]; then
            unset filter
        elif [[ ${line} = @include* ]]; then
            line=${line#@include }
            include "${dir}${line}"
        elif [[ -z ${filter+@} ]]; then
            echo "${line}"
        fi
    done <"${file}"
}

include "${file}"
