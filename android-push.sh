#!/bin/bash
set -e

adb shell mkdir -p /data/local/tmp/Cycript.lib/{l,u}
adb push cycript.and.in /data/local/tmp/cycript
adb shell chmod 755 /data/local/tmp/cycript

files=()
files+=(.libs/cycript)
files+=(.libs/libcycript.so)
files+=(libcycript.jar)
files+=(libcycript.db)
files+=(../libcycript.cy)
files+=(../android/armeabi/libJavaScriptCore.so)

for file in "${files[@]}"; do
    adb push build.and-armeabi/"${file}" /data/local/tmp/Cycript.lib
done

for term in linux unknown; do
    adb push {terminfo,/data/local/tmp/Cycript.lib}/"${term:0:1}/${term}"
done
