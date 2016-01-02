#!/usr/bin/python

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

import sys

escape = False

trees = [dict(), dict(), dict(), dict(), dict()]

def insert(point):
    point = list(point)
    tree = trees[len(point) - 1]
    for unit in point:
        unit = ord(unit)
        tree = tree.setdefault(unit, dict())

def insertmore(point, prefix=''):
    if len(point) == 0:
        return insert(prefix)

    next = point[0]
    point = point[1:]
    insertmore(point, prefix + next)

    upper = next.upper()
    if upper != next:
        insertmore(point, prefix + upper)

for line in sys.stdin:
    line = line[0:14]
    line = line.rstrip(' \n')
    line = line.split('..')
    if len(line) == 1:
        line.append(line[0])
    line = [int(end, 16) for end in line]
    for point in range(line[0], line[1] + 1):
        if escape:
            point = format(point, 'x')
            insertmore(point)
        else:
            # http://stackoverflow.com/questions/7105874/
            point = "\\U%08x" % point
            point = point.decode('unicode-escape')
            point = point.encode('utf-8')
            insert(point)

items = []

def encode(value):
    if escape:
        if ord('A') <= value <= ord('Z') or ord('a') <= value <= ord('z') or ord('0') <= value <= ord('9'):
            return chr(value)
    return '\\x%02x' % value

def build(index, tree, units, wrap=()):
    if index == 0:
        keys = sorted(tree.keys())
    else:
        keys = []
        for unit, tree in sorted(tree.items()):
            if build(index - 1, tree, units + [unit], wrap):
                keys.append(unit)

    if len(keys) == 0:
        return False

    if escape:
        if len(keys) == 10 + 6 + 6:
            return True
    else:
        if len(keys) == 0xc0 - 0x80:
            return True

    item = ''
    for unit in units:
        item += encode(unit)
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

        item += encode(first)
        if first != last:
            if last != first + 1:
                item += '-'
            item += encode(last)

        first = unit
        last = unit

    item += ']'

    if index != 0:
        if escape:
            item += '[0-9A-Fa-f]'
        else:
            item += '[\\x80-\\xbf]'
        if index != 1:
            item += '{' + str(index) + '}'

    if False:
        item = item.replace('[\\x00-\\x7f]', '{U1}')
        item = item.replace('[\\x80-\\xbf]', '{U0}')
        item = item.replace('[\\xc2-\\xdf]', '{U2}')
        item = item.replace('[\\xe0-\\xef]', '{U3}')
        item = item.replace('[\\xf0-\\xf4]', '{U4}')

    count = len(units) + 1 + index
    if wrap == ():
        if not escape:
            wrap = ('', '')
        elif count > 4:
            return False
        else:
            wrap = ('0' * (4 - count), '')

    items.append(wrap[0] + item + wrap[1])
    return False

for index, tree in enumerate(trees):
    build(index, tree, [])
    if escape:
        build(index, tree, [], ('\\{0*', '\\}'))

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
