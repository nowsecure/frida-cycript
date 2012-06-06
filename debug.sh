#!/bin/bash
flags='-g3 -O0'
export CFLAGS=$flags
export CXXFLAGS=$flags
export OBJCXXFLAGS=$flags
./configure
