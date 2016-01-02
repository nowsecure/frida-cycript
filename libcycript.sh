#!/usr/bin/env bash

# Cycript - The Truly Universal Scripting Language
# Copyright (C) 2009-2016  Jay Freeman (saurik)

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
echo "create table cache (name text not null, system int not null, flags int not null, code text not null, primary key (name, system));" | sqlite3 "${sql}"

def=$3
if [[ -n "${def}" ]]; then
    { echo "begin;"; cat "$def"; echo "commit;"; } | sed -e 's/^\([^|]*\)|\([0-9]*\)\"\(.*\)\"$/insert into cache (name, system, flags, code) values (<@<\1>@>, '"$sys"', \2, <@<\3>@>);/;s/'"'"'/'"'"''"'"'/g;s/\(<@<\|>@>\)/'"'"'/g' | sqlite3 "${sql}"
fi
