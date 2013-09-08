#!/bin/bash
set -e
for build in build.*; do
    cd "${build}"
    make
    cd ..
done
