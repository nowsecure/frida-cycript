#!/bin/bash
set -e

version=$(git describe --always --tags --dirty="+" --match="v*" | sed -e 's@-\([^-]*\)-\([^-]*\)$$@+\1.\2@;s@^v@@;s@%@~@g')

for abi in armeabi; do
    cd "build.and-${abi}"

    rm -rf Cycript.lib
    mkdir Cycript.lib
    cp -a ../cycript.and.in Cycript.lib/cycript
    chmod 755 Cycript.lib/cycript

    files=()
    files+=(.libs/cycript)
    files+=(.libs/libcycript.so)
    files+=(libcycript.jar)
    files+=(libcycript.db)
    files+=(../libcycript.cy)
    files+=(../android/armeabi/libJavaScriptCore.so)

    for file in "${files[@]}"; do
        cp -a "${file}" Cycript.lib
    done

    for term in linux unknown; do
        mkdir -p Cycript.lib/"${term:0:1}"
        cp -a {../terminfo,Cycript.lib}/"${term:0:1}/${term}"
    done

    cp -af ../cycript.and.in cycript
    chmod 755 cycript

    zip=Cycript_${version}_${abi}.zip
    rm -f "${zip}"
    zip -r9y "${zip}" cycript Cycript.lib
done
