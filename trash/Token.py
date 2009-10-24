#!/usr/bin/python

import sys

lines = sys.stdin.read().rstrip('\n').split('\n')

def data(line):
    name = line[0].replace('&', 'Ampersand').replace('^', 'Carrot').replace('=', 'Equal').replace('!', 'Exclamation').replace('-', 'Hyphen').replace('<', 'Left').replace('%', 'Percent').replace('.', 'Period').replace('|', 'Pipe').replace('+', 'Plus').replace('>', 'Right').replace('/', 'Slash').replace('*', 'Star').replace('~', 'Tilde')
    text = line[0].lower()
    word = text[0].isalpha()
    prefix = None if line[1] == '-' else line[1]
    assign = None if len(line) < 3 or line[2] != 'A' else '' if len(line) < 4 else line[3]
    infix = None if len(line) < 3 or line[2] != 'R' else line[3]
    precedence = line[4] if infix != None and len(line) > 4 else None
    postfix = infix if infix != None and precedence == None else None
    if postfix != None:
        infix = None
    return name, text, word, prefix, assign, infix, precedence, postfix

for line in lines:
    line = line.split()
    name, text, word, prefix, assign, infix, precedence, postfix = data(line)
    print '%%token <CYToken%(name)s> CYToken%(name)s "%(text)s"' % locals()
