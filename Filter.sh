#!/usr/bin/env bash

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
