#!/bin/bash
function run { sdk=$1; arch=$2; shift 2; xcrun --sdk "${sdk}" g++ -arch "${arch}" "$@" -o a \
    -isysroot "$(xcodebuild -sdk "${sdk}" -version Path)" \
    -x c <(echo "void CYListenServer(short port); int main() { CYListenServer(6667); return 0; }") \
    -framework Foundation -framework JavaScriptCore -framework Cycript; }
echo macosx
run macosx i386 -mmacosx-version-min=10.6 -F Cycript.osx "${flags[@]}"
run macosx x86_64 -mmacosx-version-min=10.6 -F Cycript.osx "${flags[@]}"
echo iphoneos
run iphoneos armv6 -miphoneos-version-min=4.0 -F Cycript.ios "${flags[@]}"
run iphoneos arm64 -miphoneos-version-min=7.0 -F Cycript.ios "${flags[@]}"
echo iphonesimulator
run iphonesimulator i386 -mios-simulator-version-min=4.0 -F Cycript.ios "${flags[@]}"
run iphonesimulator x86_64 -mios-simulator-version-min=4.0 -F Cycript.ios "${flags[@]}" -fobjc-abi-version=2 -fobjc-legacy-dispatch
