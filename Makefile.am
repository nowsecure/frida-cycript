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

.DELETE_ON_ERROR:

AUTOMAKE_OPTIONS = subdir-objects
CLEANFILES = 
SUBDIRS = 

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DYYDEBUG=1 -DCY_SYSTEM="\"$(CY_SYSTEM)\""
AM_CPPFLAGS += -include config.h -include $(srcdir)/unconfig.h

AM_CFLAGS = -fvisibility=hidden
AM_CXXFLAGS = -fvisibility=hidden
AM_OBJCXXFLAGS = -fvisibility=hidden
AM_LDFLAGS = -fvisibility=hidden

AM_OBJCXXFLAGS += -fobjc-exceptions

CY_LDFLAGS = -no-undefined -avoid-version -export-dynamic

lib_LTLIBRARIES = 

lib_LTLIBRARIES += libcycript.la
libcycript_la_LDFLAGS = $(CY_LDFLAGS)
libcycript_la_LIBADD = $(LTLIBUV) $(LTLIBFFI) $(LTLIBSQLITE3) $(LTLIBGCC) -ldl

libcycript_la_SOURCES = ConvertUTF.c Decode.cpp Driver.cpp Error.cpp Highlight.cpp Library.cpp Network.cpp Output.cpp Replace.cpp Syntax.cpp
libcycript_la_SOURCES += Parser.cpp Scanner.cpp

filters = 

if CY_CONSOLE
bin_PROGRAMS = cycript
cycript_SOURCES = Console.cpp
cycript_LDADD = libcycript.la $(LTLIBREADLINE) $(LTLIBTERMCAP) $(LTLIBGCC) $(PTHREAD_CFLAGS) -ldl
libcycript_la_SOURCES += Complete.cpp
endif

if CY_EXECUTE
libcycript_la_SOURCES += sig/ffi_type.cpp sig/parse.cpp sig/copy.cpp
libcycript_la_SOURCES += Execute.cpp JavaScriptCore.cpp
libcycript_la_LIBADD += $(LTJAVASCRIPTCORE)

AM_CPPFLAGS += -DCY_EXECUTE
filters += C

datdir = $(libdir)
dat_DATA = libcycript.db

CLEANFILES += libcycript.db
libcycript.db: Bridge.def libcycript.sh
	$(srcdir)/libcycript.sh $(CY_SYSTEM) $@ $<

if CY_PRELINK
CLEANFILES += Analyze
Analyze: Analyze.cpp Error.cpp Output.cpp Replace.cpp Syntax.cpp
	$(CXX_FOR_BUILD) $(CXXFLAGS_FOR_BUILD) -std=c++11 $(LDFLAGS_FOR_BUILD) -DCY_OBJECTIVEC=$(CY_OBJECTIVEC) -I$(srcdir)/extra -o $@ $^ $(CY_LIBCLANG) -Wno-bitwise-op-parentheses -Wno-dangling-else -Wno-logical-op-parentheses

CLEANFILES += Bridge.def
Bridge.def: Analysis.cpp Analyze
	./Analyze $< $(OBJCXX) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) >$@
else
CLEANFILES += Bridge.def
Bridge.def: Bridge.def.in
	cat $< >$@
endif

CLEANFILES += Bridge.gperf
Bridge.gperf: Bridge.def Bridge.sh
	$(srcdir)/Bridge.sh <$< >$@

CLEANFILES += Bridge.hpp
Bridge.hpp: Bridge.gperf
	$(GPERF) $< >$@
endif

if CY_JAVA
filters += Java
libcycript_la_SOURCES += Java/Execute.cpp
libcycript_la_LIBADD += $(LTJAVA)
endif

if CY_RUBY
filters += Ruby
libcycript_la_SOURCES += Ruby/Execute.cpp
libcycript_la_LIBADD += $(LTRUBY)
endif

if CY_PYTHON
filters += Python
libcycript_la_SOURCES += Python/Execute.cpp
libcycript_la_LIBADD += $(LTPYTHON)
endif

if CY_OBJECTIVEC
filters += ObjectiveC
libcycript_la_SOURCES += ObjectiveC/Output.cpp ObjectiveC/Replace.cpp ObjectiveC/Library.mm
libcycript_la_LIBADD += $(LTOBJECTIVEC)
endif

if CY_ATTACH
libcycript_la_SOURCES += Handler.cpp

if CY_CONSOLE
cycript_SOURCES += Inject.cpp
AM_CPPFLAGS += -DCY_ATTACH
endif
endif

CLEANFILES += Parser.ypp
Parser.ypp: Parser.ypp.in
	$(srcdir)/Filter.sh $< >$@ $(filters)

CLEANFILES += Scanner.lpp
Scanner.lpp: Scanner.lpp.in UnicodeIDStart.l UnicodeIDContinue.l
	$(srcdir)/Filter.sh $< >$@ $(filters)

CLEANFILES += Scanner.cpp Scanner.output lex.backup
Scanner.cpp: Scanner.lpp
	$(FLEX) $(LFLAGS) -o $@ -T $< 2>Scanner.output || (grep -F '$<:' Scanner.output; false)
	@#grep -E '^(No backing up\.|Compressed tables always back up\.)$$' lex.backup >/dev/null
	! grep -n '^ jam-transitions: ' lex.backup | grep -v ': EOF \[\(\]\| \\2\)'
	! grep -F ': warning, ' Scanner.output || true

Scanner.lo: Parser.hpp IdentifierStart.h IdentifierContinue.h
Parser.lo: Parser.hpp
Parser.cpp: Parser.hpp

CLEANFILES += Parser.cpp Parser.hpp stack.hh Parser.output
Parser.hpp: Parser.ypp
	$(BISON) -v --report=state -Werror -o Parser.cpp $<
	! grep -n '^State [0-9]* conflicts:' Parser.output
	! grep -n '^ *.* *reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Parser.output | grep -v '$$default'
	! grep -n '^ *$$default *reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Parser.output -B 2 | grep 'shift, and go to state [0-9]*$$'
	! grep -n '^ *$$default *reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Parser.output -B 2 | grep -v '$$default' | grep 'reduce using rule [0-9]* ([^)]*)$$'
	perl -pi -e 's/yytranslate_ \(yylex \(/(yylex_ (/g' Parser.cpp
