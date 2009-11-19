#!/usr/bin/env bash

cat << EOF
%{
#include <cstring>
#include "Execute.hpp"
%}

%language=ANSI-C

%define lookup-function-name CYBridgeHash_
%define slot-name name_

%struct-type
%omit-struct-type

%pic

struct CYBridgeEntry {
    int name_;
    const char *value_;
    void *cache_;
};

%%
EOF

grep '^[CFV]' "$1" | sed -e 's/^C/0/;s/^F/1/;s/^V/2/' | sed -e 's/"/\\"/g;s/^\([^ ]*\) \([^ ]*\) \(.*\)$/\1\2, "\3", NULL/';
grep '^[EST]' "$1" | sed -e 's/^S/3/;s/^T/4/;s/^E/5/' | sed -e 's/^5\(.*\)$/4\1 i/;s/"/\\"/g' | sed -e 's/^\([^ ]*\) \([^ ]*\) \(.*\)$/\1\2, "\3", NULL/';
grep '^:' "$1" | sed -e 's/^: \([^ ]*\) \(.*\)/6\1, "\2", NULL/';
