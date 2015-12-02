#!/bin/bash
./apple-make.sh build-osx-i386
echo "backup $(grep -c '^State ' build.osx-i386/lex.backup)"
echo "states $(grep '^static .* yy_accept\[' build.osx-i386/Scanner.cpp | sed -e 's/.*\[//;s/].*//') 3528"
echo "jammed $(grep -F 'accepts: ['"$(grep 'jammed' build.osx-i386/Scanner.cpp -B 3 | head -n 1 | sed -e 's/:$//;s/.* //')"']' build.osx-i386/Scanner.output | sed -e 's/.* # //;s/ .*//')"
