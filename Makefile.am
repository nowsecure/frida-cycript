SHELL = @_BASH@

.DELETE_ON_ERROR:

AUTOMAKE_OPTIONS = subdir-objects
CLEANFILES = 
SUBDIRS = 

ACLOCAL_AMFLAGS = -I m4

# XXX: do I really need -fno-common?
AM_CPPFLAGS = -Werror -fno-common -I$(srcdir)/include -DYYDEBUG=1

lib_LTLIBRARIES = libcycript.la
libcycript_la_SOURCES = Highlight.cpp Network.cpp Output.cpp Parser.cpp Replace.cpp
libcycript_la_LDFLAGS = -no-undefined $(LTLIBAPR) $(LTLIBICONV) -ldl

libcycript_la_SOURCES += Cycript.tab.cc
libcycript_la_SOURCES += lex.cy.cpp

bin_PROGRAMS = cycript
cycript_SOURCES = Console.cpp Display.cpp
cycript_LDFLAGS = $(LTLIBAPR) -ldl
cycript_LDADD = libcycript.la

ldid = true
entitle = $(ldid) -Scycript.xml

filters = $(CY_FILTERS)

if CY_EXECUTE
libcycript_la_SOURCES += sig/ffi_type.cpp sig/parse.cpp sig/copy.cpp
libcycript_la_SOURCES += Bridge.cpp Execute.cpp JavaScriptCore.cpp Library.cpp

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

if CY_OBJECTIVEC
filters += ObjectiveC
libcycript_la_SOURCES += ObjectiveC/Output.mm ObjectiveC/Replace.cpp ObjectiveC/Library.mm

ObjectiveC/Library.lo: Struct.hpp

# XXX: this is not architecture-specific... isn't this just totally wrong?!
# XXX: clang doesn't support this feature... that makes me really very sad.
CLEANFILES += Struct.hpp
Struct.hpp:
	$$($(OBJCXX) -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@
endif

if CY_MACH
libcycript_la_SOURCES += Handler.mm
cycript_SOURCES += Mach/Inject.cpp

AM_CPPFLAGS += -DCY_ATTACH -DCY_LIBRARY='"$(libdir)/libcycript.$(SO)"'

Mach/Inject.$(OBJEXT): Trampoline.t.hpp

CLEANFILES += Trampoline.t.lo
Trampoline.t.lo: Trampoline.t.cpp Trampoline.hpp Baton.hpp Standard.hpp
	$(LTCXXCOMPILE) $(CY_ARCH) -c -o $@ $< -fno-stack-protector -fno-exceptions -O0

CLEANFILES += libTrampoline.t.la
libTrampoline.t.la: Trampoline.t.lo
	$(CXXLINK) $(CY_ARCH) -rpath $(libdir) $^

CLEANFILES += Trampoline.t.hpp
Trampoline.t.hpp: libTrampoline.t.la trampoline.sh
	$(srcdir)/trampoline.sh $@ .libs/libTrampoline.t.dylib Trampoline $(SED) $(LIPO) $(NM) $(OTOOL)
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

Highlight.lo: Cycript.tab.hh

CLEANFILES += Cycript.tab.cc Cycript.tab.hh location.hh position.hh stack.hh Cycript.output
Cycript.tab.cc Cycript.tab.hh location.hh position.hh stack.hh Cycript.output: Cycript.yy
	$(BISON) -v --report=state $<
	! grep -n '^ *$$default  reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Cycript.output -B 2 | grep 'shift, and go to state [0-9]*$$'
	! grep -n '^ *$$default  reduce using rule [0-9]* (Lex[A-Z][^)]*)$$' Cycript.output -B 2 | grep -v '$$default' | grep 'reduce using rule [0-9]* ([^)]*)$$'
