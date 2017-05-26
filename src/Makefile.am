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
AM_CPPFLAGS += -include config.h

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
libcycript_la_LIBADD = $(LTLIBSQLITE3) $(LTLIBGCC) -ldl

libcycript_la_SOURCES = ConvertUTF.c Decode.cpp Driver.cpp Error.cpp Highlight.cpp Library.cpp Output.cpp Replace.cpp Syntax.cpp
libcycript_la_SOURCES += Parser.cpp Scanner.cpp
libcycript_la_SOURCES += sig/parse.cpp sig/copy.cpp

filters = C Java Ruby Python ObjectiveC

if CY_CONSOLE
bin_PROGRAMS = cycript
cycript_SOURCES = Console.cpp
cycript_LDADD = libcycript.la $(LTLIBREADLINE) $(LTLIBTERMCAP) $(LTLIBGCC) $(PTHREAD_CFLAGS) -ldl
libcycript_la_SOURCES += Complete.cpp
endif

if CY_EXECUTE
BUILT_SOURCES = $(builddir)/frida/libfrida-core.a

AM_CPPFLAGS += -I$(srcdir)/frida
CY_LDFLAGS += -L$(builddir)/frida

libcycript_la_SOURCES += Execute.cpp
libcycript_la_LIBADD += @FRIDA_LIBS@
libcycript_la_LDFLAGS += @FRIDA_LDFLAGS@
libcycript_la_DEPENDENCIES = frida/libfrida-core.a libcycript-runtime.js
endif
dat_DATA += libcycript.db

CLEANFILES += libcycript.db
libcycript.db: Bridge.def libcycript.py
	$(srcdir)/libcycript.py $(CY_SYSTEM) $@ $(top_srcdir)/ext <$<

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

if CY_JAVA
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
libcycript_la_SOURCES += Ruby/Execute.cpp
libcycript_la_LIBADD += $(LTRUBY)
endif

if CY_PYTHON
libcycript_la_SOURCES += Python/Execute.cpp
libcycript_la_LIBADD += $(LTPYTHON)
endif

AM_CPPFLAGS += -DCY_OBJECTIVEC
libcycript_la_SOURCES += ObjectiveC/Output.cpp ObjectiveC/Replace.cpp
if CY_OBJECTIVEC
libcycript_la_LIBADD += $(LTOBJECTIVEC)
endif

libcycript_la_LIBTOOLFLAGS = --tag=CXX

CLEANFILES += Parser.ypp
Parser.ypp: Parser.ypp.in
	$(srcdir)/Filter.sh $< >$@ $(filters)

CLEANFILES += Scanner.lpp
Scanner.lpp: Scanner.lpp.in UnicodeIDStart.l UnicodeIDContinue.l
	$(srcdir)/Filter.sh $< >$@ $(filters)

CLEANFILES += Scanner.cpp Scanner.output lex.backup
Scanner.cpp: Scanner.lpp
	$(FLEX) $(LFLAGS) -o $@ -T $< 2>Scanner.output || (grep -F '$<:' Scanner.output; false)

Scanner.lo: Parser.hpp IdentifierStart.h IdentifierContinue.h
Parser.lo: Parser.hpp
Parser.cpp: Parser.hpp

CLEANFILES += Parser.cpp Parser.hpp stack.hh Parser.output
Parser.hpp: Parser.ypp
	@BISON_PKGDATADIR_OVERRIDE@ $(BISON) -v --report=state -Werror -o Parser.cpp $<
	perl -pi -e 's/yytranslate_ \(yylex \(/(yylex_ (/g' Parser.cpp

frida_version = 10.0.8

$(builddir)/frida/libfrida-core.a:
	mkdir -p $(@D)/_
	curl -Ls https://github.com/frida/frida/releases/download/$(frida_version)/frida-core-devkit-$(frida_version)-@FRIDA_OS_ARCH@.tar.xz | xz -d | tar -C $(@D)/_ -xf -
	mv $(@D)/_/* $(@D)
	rmdir $(@D)/_

libcycript-runtime.js: Agent/index.js node_modules
	node_modules/.bin/frida-compile Agent -o libcycript-runtime.js

node_modules:
	npm install
