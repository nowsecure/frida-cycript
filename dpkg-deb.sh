#!/bin/bash
flags=()
for Z in lzma bzip2 gzip; do
    if dpkg-deb -Z"$Z" -h &>/dev/null && which "$Z" &>/dev/null; then
        flags+=(-Z"$Z")
        break
    fi
done
fauxsu dpkg-deb "${flags[@]}" "$@"
