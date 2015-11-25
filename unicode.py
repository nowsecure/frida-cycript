#!/usr/bin/python

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

import sys

trees = [dict(), dict(), dict(), dict()]

for line in sys.stdin:
    line = line[0:14]
    line = line.rstrip(' \n')
    line = line.split('..')
    if len(line) == 1:
        line.append(line[0])
    line = [int(end, 16) for end in line]
    for point in range(line[0], line[1] + 1):
        # http://stackoverflow.com/questions/7105874/
        point = "\\U%08x" % point
        point = point.decode('unicode-escape')
        point = point.encode('utf-8')
        point = list(point)
        tree = trees[len(point) - 1]
        for unit in point:
            unit = ord(unit)
            tree = tree.setdefault(unit, dict())

items = []

def build(index, tree, units):
    if index == 0:
        keys = tree.keys()
    else:
        keys = []
        for unit, tree in tree.iteritems():
            if build(index - 1, tree, units + [unit]):
                keys.append(unit)

    if len(keys) == 0:
        return False
    if len(keys) == 0xc0 - 0x80:
        return True

    item = ''
    for unit in units:
        item += '\\x%02x' % unit
    item += '['

    first = -1
    last = -1

    assert len(keys) != 0
    for unit in keys + [-1]:
        if unit != -1:
            if first == -1:
                first = unit
                last = unit
                continue
            if unit == last + 1:
                last = unit
                continue

        item += '\\x%02x' % first
        if first != last:
            if last != first + 1:
                item += '-'
            item += '\\x%02x' % last

        first = unit
        last = unit

    item += ']'

    for i in range(0, index):
        item += '[\\x80-\\xbf]'

    items.append(item)
    return False

for index, tree in enumerate(trees):
    build(index, tree, [])

name = sys.argv[1]
parts = []
part = []
length = 0
index = 0
for item in items:
    part += [item]
    length += len(item) + 1
    if length > 1000:
        indexed = name + '_' + str(index)
        index += 1
        print indexed, '|'.join(part)
        parts += ['{' + indexed + '}']
        part = []
        length = 0
parts += part
print name, '|'.join(parts)
