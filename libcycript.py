#!/usr/bin/python

import os
import sqlite3
import sys

keys = {}

for db in sys.argv[2:]:
    with sqlite3.connect(db) as sql:
        c = sql.cursor()
        for name, system, flags, code in c.execute('select name, system, flags, code from cache'):
            key = (name, flags, code)
            keys[key] = keys.get(key, 0) | system

db = sys.argv[1]
with sqlite3.connect(db) as sql:
    many = []
    for key, system in keys.items():
        name, flags, code = key
        many.append((name, system, flags, code))
    c = sql.cursor()
    c.executemany("insert into cache (name, system, flags, code) values (?, ?, ?, ?)", many)
