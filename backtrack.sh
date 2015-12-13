#!/bin/bash
./apple-make.sh build-osx-i386
echo
echo "backup" $(grep -c '^State ' build.osx-i386/lex.backup)
echo "states" $(grep '^static .* yy_accept\[' build.osx-i386/Scanner.cpp | sed -e 's/.*\[//;s/].*//') 3680
echo "jammed" $(grep -F 'accepts: ['"$(grep 'jammed' build.osx-i386/Scanner.cpp -B 3 | head -n 1 | sed -e 's/:$//;s/.* //')"']' build.osx-i386/Scanner.output | sed -e 's/.* # //;s/ .*//')
echo "failed" $(grep "^ jam-transitions: " build.osx-i386/lex.backup | grep -v ': EOF \[\(\]\| \\2\)' | wc -l)
echo
grep '^ jam-transitions: EOF \[ \\2' build.osx-i386/lex.backup -B 2 | grep $'^\t' | sort | uniq -c
