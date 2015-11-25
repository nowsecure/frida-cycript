#!/bin/bash
./apple-make.sh
grep '^State' build.osx-i386/lex.backup | wc -l
