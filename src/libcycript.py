#!/usr/bin/python

import os
import sqlite3
import sys

system = sys.argv[1]
dbfile = sys.argv[2]
nodejs = sys.argv[3]
merges = sys.argv[4:]

system = int(system)
nodejs += '/node/lib'

keys = {}

while True:
    line = sys.stdin.readline()
    if line == "":
        break
    elif line == "\n":
        continue
    assert line[-1] == '\n'
    line = line[0:-1]

    pipe = line.index('|')
    name = line[0:pipe]
    line = line[pipe+1:]

    quote = line.index('"')
    flags = int(line[0:quote])
    code = line[quote+1:-1]

    key = (name, flags, code)
    keys[key] = system

for db in merges:
    with sqlite3.connect(db) as sql:
        c = sql.cursor()
        for name, system, flags, code in c.execute('select name, system, flags, code from cache'):
            key = (name, flags, code)
            keys[key] = keys.get(key, 0) | system

if os.path.exists(dbfile):
    os.unlink(dbfile)

with sqlite3.connect(dbfile) as sql:
    c = sql.cursor()

    c.execute("create table cache (name text not null, system int not null, flags int not null, code text not null, primary key (name, system))")
    c.execute("create table module (name text not null, flags int not null, code blob not null, primary key (name))")

    for name in [js[0:-3] for js in os.listdir(nodejs) if js.endswith('.js')]:
        with open(nodejs + '/' + name + '.js', 'r') as file:
            code = file.read()
        c.execute("insert into module (name, flags, code) values (?, ?, ?)", [name, 0, buffer(code)])

    many = []
    for key, system in keys.items():
        name, flags, code = key
        many.append((name, system, flags, code))
    c.executemany("insert into cache (name, system, flags, code) values (?, ?, ?, ?)", many)
