#!/usr/bin/env bash

# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2015  Jay Freeman (saurik)

# GNU Affero General Public License, Version 3 {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# }}}

cat << EOF
%{
#include <cstddef>
#include <cstring>
#include "Execute.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif
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

cat <<EOF
%%
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
EOF
