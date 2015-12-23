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

set -e

sys=$1
sql=$2

rm -f "${sql}"
echo "create table cache (name text not null, system int not null, value text not null, primary key (name, system));" | sqlite3 "${sql}"

def=$3
if [[ -n "${def}" ]]; then
    { echo "begin;"; cat "$def"; echo "commit;"; } | sed -e 's/^\([^|]*\)|\"\(.*\)\"$/insert into cache (name, system, value) values (<@<\1>@>, '"$sys"', <@<\2>@>);/;s/'"'"'/'"'"''"'"'/g;s/\(<@<\|>@>\)/'"'"'/g' | sqlite3 "${sql}"
fi
