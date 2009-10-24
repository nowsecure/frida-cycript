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

def express(expression, type, args, call):
    print 'struct CYExpression%(expression)s :' % locals()
    print '    CYExpression%(type)s' % locals()
    print '{'
    print '    CYExpression%(expression)s(%(args)s) :' % locals()
    print '        CYExpression%(type)s(%(call)s)' % locals()
    print '    {'
    print '    }'
    print '};'
    print

for line in lines:
    line = line.split()
    name, text, word, prefix, assign, infix, precedence, postfix = data(line)

    print 'struct CYToken%(name)s :' % locals()
    if prefix != None:
        print '    CYTokenPrefix,'
    if infix != None:
        print '    CYTokenInfix,'
    if postfix != None:
        print '    CYTokenPostfix,'
    if assign != None:
        print '    CYTokenAssignment,'
    if word:
        print '    CYTokenWord,'
    print '    virtual CYToken'
    print '{'
    print '    virtual const char *Text() const {'
    print '        return "%(text)s";' % locals()
    print '    }'
    if precedence != None or prefix != None or assign != None or infix != None:
        print
    if precedence != None:
        print '    virtual unsigned Precedence() const {'
        print '        return %(precedence)s;' % locals()
        print '    }'
        print
    if prefix != None:
        print '    virtual CYExpression *PrefixExpression(apr_pool_t *pool, CYExpression *rhs) const;'
    if infix != None:
        print '    virtual CYExpression *InfixExpression(apr_pool_t *pool, CYExpression *lhs, CYExpression *rhs) const;'
    if postfix != None:
        print '    virtual CYExpression *PostfixExpression(apr_pool_t *pool, CYExpression *lhs) const;'
    if assign != None:
        print '    virtual CYExpression *AssignmentExpression(apr_pool_t *pool, CYExpression *lhs, CYExpression *rhs) const;'
    print '};'
    print
    if prefix != None:
        express(prefix, 'Prefix', 'CYExpression *rhs', 'rhs')
    if infix != None:
        express(infix, 'Infix', 'CYExpression *lhs, CYExpression *rhs', 'lhs, rhs')
    if postfix != None:
        express(postfix, 'Postfix', 'CYExpression *lhs', 'lhs')
    if assign != None:
        express('Assign' + assign, 'Assignment', 'CYExpression *lhs, CYExpression *rhs', 'lhs, rhs')

for line in lines:
    line = line.split()
    name, text, word, prefix, assign, infix, precedence, postfix = data(line)

    if prefix != None:
        print 'CYExpression *CYToken%(name)s::PrefixExpression(apr_pool_t *pool, CYExpression *rhs) const {' % locals()
        print '    return new(pool) CYExpression%(prefix)s(rhs);' % locals()
        print '}'
        print
    if infix != None:
        print 'CYExpression *CYToken%(name)s::InfixExpression(apr_pool_t *pool, CYExpression *lhs, CYExpression *rhs) const {' % locals()
        print '    return new(pool) CYExpression%(infix)s(lhs, rhs);' % locals()
        print '}'
        print
    if postfix != None:
        print 'CYExpression *CYToken%(name)s::PostfixExpression(apr_pool_t *pool, CYExpression *rhs) const {' % locals()
        print '    return new(pool) CYExpression%(postfix)s(rhs);' % locals()
        print '}'
        print
    if assign != None:
        print 'CYExpression *CYToken%(name)s::AssignmentExpression(apr_pool_t *pool, CYExpression *lhs, CYExpression *rhs) const {' % locals()
        print '    return new(pool) CYExpressionAssign%(assign)s(lhs, rhs);' % locals()
        print '}'
        print
