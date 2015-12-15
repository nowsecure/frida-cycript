#!/bin/bash
echo -ne "bool Is$1(unsigned v){return false"
sed -e 's/ .*//;/\.\./!{s/^/||v==0x/};/\.\./{s//\&\&0x/;s/^/||v>=0x/;s/$/>=v/};' | tr -d $'\n'
echo ";}"
