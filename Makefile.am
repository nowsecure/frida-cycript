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

.DELETE_ON_ERROR:

AUTOMAKE_OPTIONS = subdir-objects
CLEANFILES = 
SUBDIRS = 

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DYYDEBUG=1 -DCY_SYSTEM="\"$(CY_SYSTEM)\""
AM_CPPFLAGS += -include config.h -include $(srcdir)/unconfig.h

CY_FLAGS = 
CY_FLAGS += -fvisibility=hidden

if CY_ANDROID
CY_FLAGS += -fPIE
endif

AM_CFLAGS = $(CY_FLAGS)
AM_CXXFLAGS = $(CY_FLAGS)
AM_OBJCXXFLAGS = $(CY_FLAGS)
AM_LDFLAGS = $(CY_FLAGS)

if CY_ANDROID
AM_LDFLAGS += -rdynamic -fPIE -pie
endif

AM_OBJCXXFLAGS += -fobjc-exceptions

CY_LDFLAGS = -no-undefined -avoid-version -export-dynamic

datdir = $(libdir)
dat_DATA = 

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

if CY_ANDROID
bin_PROGRAMS += cycript-pie
cycript_pie_SOURCES = pie.c
cycript_pie_LDFLAGS = -fno-PIE
endif
endif

if CY_EXECUTE
libcycript_la_SOURCES += sig/ffi_type.cpp sig/parse.cpp sig/copy.cpp
libcycript_la_SOURCES += Execute.cpp JavaScriptCore.cpp
libcycript_la_LIBADD += $(LTJAVASCRIPTCORE)
dat_DATA += libcycript.db

AM_CPPFLAGS += -DCY_EXECUTE
filters += C

CLEANFILES += libcycript.db
libcycript.db: Bridge.def libcycript.py
	$(srcdir)/libcycript.py $(CY_SYSTEM) $@ $(srcdir) <$<

if CY_PRELINK
CY_LANGFLAGS = -DCY_JAVA=$(CY_JAVA) -DCY_PYTHON=$(CY_PYTHON) -DCY_OBJECTIVEC=$(CY_OBJECTIVEC)

if CY_ANDROID
CY_ANDFLAGS = -U__APPLE__ -D__ANDROID__
else
CY_ANDFLAGS = 
endif

CLEANFILES += Analyze
Analyze: Analyze.cpp Error.cpp Output.cpp Replace.cpp Syntax.cpp
	$(CXX_FOR_BUILD) $(CXXFLAGS_FOR_BUILD) -std=c++11 $(LDFLAGS_FOR_BUILD) $(CY_LANGFLAGS) -I$(srcdir)/extra -o $@ $^ $(CY_LIBCLANG) -Wno-bitwise-op-parentheses -Wno-dangling-else -Wno-logical-op-parentheses

CLEANFILES += Bridge.def
Bridge.def: Analysis.cpp Analyze Bridge.old
	./Analyze $< $(OBJCXX) $(CPPFLAGS) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(CY_LANGFLAGS) $(CY_ANDFLAGS) >$@
	cat $(srcdir)/Bridge.old >>$@
else
CLEANFILES += Bridge.def
Bridge.def: Bridge.def.in
	cat $< >$@
endif

#CLEANFILES += Hasher.gperf
#Hasher.gperf: Hasher.def Hasher.sh
#	$(srcdir)/Hasher.sh <$< >$@

#CLEANFILES += Hasher.hpp
#Hasher.hpp: Hasher.gperf
#	$(GPERF) $< >$@
endif

if CY_JAVA
filters += Java
libcycript_la_SOURCES += Java/Execute.cpp
dat_DATA += libcycript.jar

CLEANFILES += Class
Class/Cycript.class: Java/Cycript.java
	rm -rf Class; mkdir Class; javac -g -Xlint:unchecked -source 1.5 -target 1.5 -d Class -sourcepath $(srcdir)/Java $<
Class/classes.dex: Class/Cycript.class
	cd Class; dx --dex --output=../$@ *.class

CLEANFILES += libcycript.jar
libcycript.jar: Class/classes.dex Class/Cycript.class
	cd Class; jar cf ../$@ *
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
AM_CPPFLAGS += -DCY_OBJECTIVEC
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
