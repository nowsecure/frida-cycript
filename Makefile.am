# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2014  Jay Freeman (saurik)

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

AM_CPPFLAGS = -DYYDEBUG=1
AM_CPPFLAGS += -include config.h -include $(srcdir)/unconfig.h

CY_LDFLAGS = -no-undefined -avoid-version -export-dynamic

lib_LTLIBRARIES = 

lib_LTLIBRARIES += libcycript.la
libcycript_la_LDFLAGS = $(CY_LDFLAGS)
libcycript_la_LIBADD = $(LTLIBFFI) $(LTLIBGCC) -ldl

libcycript_la_SOURCES = ConvertUTF.c Decode.cpp Driver.cpp Highlight.cpp Library.cpp Network.cpp Output.cpp Parser.cpp Replace.cpp
libcycript_la_SOURCES += Cycript.tab.cc lex.cy.cpp

filters = 

if CY_CONSOLE
bin_PROGRAMS = cycript
cycript_SOURCES = Console.cpp Display.cpp
cycript_LDADD = libcycript.la $(LTLIBREADLINE) $(LTLIBTERMCAP) $(LTLIBGCC) $(PTHREAD_CFLAGS) -ldl
endif

if CY_EXECUTE
libcycript_la_SOURCES += sig/ffi_type.cpp sig/parse.cpp sig/copy.cpp
libcycript_la_SOURCES += Bridge.cpp Execute.cpp JavaScriptCore.cpp
libcycript_la_LIBADD += $(LTJAVASCRIPTCORE)

AM_CPPFLAGS += -DCY_EXECUTE
filters += C

Bridge.lo: Bridge.hpp

CLEANFILES += Bridge.gperf
Bridge.gperf: Bridge.def Bridge.sh
	$(srcdir)/Bridge.sh $< >$@

CLEANFILES += Bridge.hpp
Bridge.hpp: Bridge.gperf
	$(GPERF) $< | $(SED) -e 's/defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__/0/' >$@
endif

if CY_JAVA
filters += Java
libcycript_la_SOURCES += Java/Execute.cpp
libcycript_la_LIBADD += $(LTJAVA)
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

CLEANFILES += Cycript.yy
Cycript.yy: Cycript.yy.in
	$(srcdir)/Filter.sh <$< >$@ $(filters)

CLEANFILES += Cycript.l
Cycript.l: Cycript.l.in
	$(srcdir)/Filter.sh <$< >$@ $(filters)

CLEANFILES += lex.cy.cpp
lex.cy.cpp: Cycript.l
	$(FLEX) -t $< | $(SED) -e 's/int yyl;/yy_size_t yyl;/;s/int yyleng_r;/yy_size_t yyleng_r;/' >$@

Console.$(OBJEXT) Cycript.tab.lo Driver.lo Handler.lo Highlight.lo Library.lo lex.cy.lo: Cycript.tab.hh

CLEANFILES += Cycript.tab.cc Cycript.tab.hh stack.hh Cycript.output
Cycript.tab.cc Cycript.tab.hh stack.hh Cycript.output: Cycript.yy
	$(BISON) -v --report=state $<
	! grep -n '^State [0-9]* conflicts:' Cycript.output
	! grep -n '^ *$$default  reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Cycript.output -B 2 | grep 'shift, and go to state [0-9]*$$'
	! grep -n '^ *$$default  reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Cycript.output -B 2 | grep -v '$$default' | grep 'reduce using rule [0-9]* ([^)]*)$$'
