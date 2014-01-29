#!/usr/bin/env bash

# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2013  Jay Freeman (saurik)

# GNU General Public License, Version 3 {{{
#
# Cycript is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# Cycript is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
# }}}

cat << EOF
%{
#include <cstddef>
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
