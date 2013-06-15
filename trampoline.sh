#!/bin/bash

set -e

hpp=$1
object=$2
name=$3
sed=$4
lipo=$5
nm=$6
otool=$7

exec >"${hpp}"

detailed=$("${lipo}" -detailed_info "${object}")

regex=$'\nNon-fat file: .* is architecture: (.*)'
if [[ ${detailed} =~ ${regex} ]]; then
    archs=(${BASH_REMATCH[1]})
    unset detailed
else
    archs=($(echo "${detailed}" | "${sed}" -e '/^architecture / { s/^architecture //; p; }; d;'))
fi

echo '#include "Trampoline.hpp"'

for arch in "${archs[@]}"; do
    if [[ "${detailed+@}" ]]; then
        offset=$(echo "${detailed}" | "${sed}" -e '
            /^architecture / { x; s/.*/0/; x; };
            /^architecture '${arch}'$/ { x; s/.*/1/; x; };
            x; /^1$/ { x; /^ *offset / { s/^ *offset //; p; }; x; }; x;
            d;
        ')
    else
        offset=0
    fi

    file=($("${otool}" -arch "${arch}" -l "${object}" | "${sed}" -e '
        x; /^1$/ { x;
            /^ *fileoff / { s/^.* //; p; };
            /^ *filesize / { s/^.* //; p; };
        x; }; x;

        /^ *cmd LC_SEGMENT/ { x; s/.*/1/; x; };

        d;
    '))

    fileoff=${file[0]}
    filesize=${file[1]}

    echo
    echo "static const char ${name}_${arch}_data_[] = {"

    od -v -t x1 -t c -j "$((offset + fileoff))" -N "${filesize}" "${object}" | "${sed}" -e '
        /^[0-7]/ ! {
            s@^        @//  @;
            s/\(....\)/ \1/g;
            s@^ // @//@;
            s/ *$/,/;
        };

        /^[0-7]/ {
            s/^[^ ]*//;
            s/  */ /g;
            s/^ *//;
            s/ $//;
            s/ /,/g;
            s/\([^,][^,]\)/0x\1/g;
            s/$/,/;
            /^,$/ ! { s/^/    /g; p; }; d;
        };
    '

    echo "};"

    echo
    entry=$("${nm}" -arch "${arch}" "${object}" | "${sed}" -e '/ _Start$/ { s/ .*//; p; }; d;')
    entry=${entry##*(0)}
    echo "static size_t ${name}_${arch}_entry_ = 0x${entry:=0};"

    echo
    echo "/*"
    "${otool}" -arch "${arch}" -vVt "${object}"
    echo "*/"

    echo
    echo "static Trampoline ${name}_${arch}_ = {"
    echo "    ${name}_${arch}_data_,"
    echo "    sizeof(${name}_${arch}_data_),"
    echo "    ${name}_${arch}_entry_,"
    echo "};"

done
