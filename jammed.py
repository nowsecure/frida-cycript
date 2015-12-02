#!/usr/bin/python

import re
import sys

tables = dict()
defines = dict()

with open(sys.argv[1], 'r') as file:
    while True:
        line = file.readline()
        if line == '':
            break

        define = re.match('#define ([A-Za-z_]*) ([0-9]+)', line)
        if define != None:
            defines[define.group(1)] = int(define.group(2))
            continue

        if not line.startswith('static yyconst '):
            continue

        end = line.index('[')
        begin = line.rindex(' ', 0, end)
        name = line[begin+1:end]

        code = ''
        while True:
            line = file.readline()
            code += line
            if line.find(';') != -1:
                break

        code = code.replace('{', '[')
        code = code.replace('}', ']')
        code = code.replace(';', ' ')
        tables[name] = eval(code)

yy_nxt = tables['yy_nxt']
yy_accept = tables['yy_accept']
yy_ec = tables['yy_ec']

YY_NUM_RULES = defines['YY_NUM_RULES']
try:
    jammed = yy_accept.index(YY_NUM_RULES)
except ValueError:
    sys.exit(0)

equivs = dict()
for ordinal, equiv in enumerate(yy_ec):
    equivs[equiv] = equivs.get(equiv, '') + chr(ordinal)

starts = set(range(1, len(yy_nxt)))
for source, table in enumerate(yy_nxt):
    if source == 0:
        continue
    for equiv, target in enumerate(table):
        if target in starts:
            starts.remove(target)

after = dict()
after[jammed] = [[]]

while True:
    finish = dict()
    for start in starts:
        suffix = after.get(start)
        if suffix == None:
            continue
        finish[start] = [[equivs[c] for c in s] for s in suffix]
    if len(finish) != 0:
        print finish
        break

    before = after
    after = dict()

    for source, table in enumerate(yy_nxt):
        if source == 0:
            continue
        string = []
        for equiv, target in enumerate(table):
            suffix = before.get(target)
            if suffix == None:
                continue
            string.extend([[equiv] + s for s in suffix])
        if len(string) == 0:
            continue
        after[source] = string

sys.exit(1)
