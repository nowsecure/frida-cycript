#!/usr/bin/python

import os
import sqlite3
import sys

keys = {}

for db in sys.argv[2:]:
    with sqlite3.connect(db) as sql:
        c = sql.cursor()
        for name, system, value in c.execute('select name, system, value from cache'):
            key = (name, value)
            keys[key] = keys.get(key, 0) | system

db = sys.argv[1]
with sqlite3.connect(db) as sql:
    many = []
    for key, system in keys.items():
        name, value = key
        many.append((name, system, value))
    c = sql.cursor()
    c.executemany("insert into cache (name, system, value) values (?, ?, ?)", many)
