#!/bin/bash
set -e
for file in .libs/cycript .libs/libcycript.so libcycript.jar libcycript.db ../libcycript.cy; do
    adb push build.and-armeabi/"${file}" /data/local/tmp/
done
