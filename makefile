SHELL := $(shell which bash 2>/dev/null)

ifndef PKG_TARG
target :=
else
target := $(PKG_TARG)-
endif

gcc := g++
flags ?= -g3 -O0 -DYYDEBUG=1

paths := $(foreach path,$(paths),$(wildcard $(path)))
flags += $(foreach path,$(paths),-I$(path) -L$(path))
objc :=

svn := $(shell svnversion)

all:
all := cycript

dpkg_architecture := $(shell which dpkg-architecture 2>/dev/null)
ifneq ($(dpkg_architecture),)
arch := $(shell $(dpkg_architecture) -qDEB_HOST_ARCH 2>/dev/null)
endif

header := Cycript.tab.hh Parser.hpp Pooling.hpp cycript.hpp Internal.hpp Error.hpp String.hpp Exception.hpp Standard.hpp

code := 
code += Replace.o Output.o
code += Cycript.tab.o lex.cy.o
code += Network.o Parser.o
code += JavaScriptCore.o Library.o

inject := 

filters := #E4X
ldid := true
entitle := $(ldid)
lib := lib
dll := so
apr := $(shell apr-1-config --link-ld)
library := $(apr)
console := $(apr) -lreadline
depends :=

restart ?= $(MAKE)
uname_s ?= $(shell uname -s)
uname_p ?= $(shell uname -p)

-include $(uname_s).mk
-include $(uname_s)-$(uname_p).mk

ifneq ($(shell pkg-config libffi --modversion 2>/dev/null),)
flags += $(shell pkg-config --cflags libffi)
endif

ifdef CY_EXECUTE
ifeq ($(filter ObjectiveC,$(filters)),)
ifneq ($(shell which gnustep-config 2>/dev/null),)
include GNUstep.mk
endif
endif
endif

flags += -Wall -Werror -Wno-parentheses #-Wno-unused
flags += -fno-common
flags += -I. -Iinclude -I$(shell apr-1-config --includedir)

all += $(lib)cycript.$(dll)

filters += $(shell bison <(echo '%code{}%%_:') -o/dev/null 2>/dev/null && echo Bison24 || echo Bison23)

ifdef arch
deb := $(shell grep ^Package: control.in | cut -d ' ' -f 2-)_$(shell grep ^Version: control.in | cut -d ' ' -f 2 | sed -e 's/\#/$(svn)/')_$(arch).deb

all:

extra::

ifeq ($(depends)$(dll),dylib)
control.tmp: control.in cycript $(lib)cycript.dylib
	sed -e 's/&/'"$$(dpkg-query -S $$(otool -lah cycript *.dylib | grep dylib | grep -v ':$$' | sed -e 's/^ *name //;s/ (offset [0-9]*)$$//' | sort -u) 2>/dev/null | sed -e 's/:.*//; /^cycript$$/ d; s/$$/,/' | sort -u | tr '\n' ' ')"'/;s/, $$//;s/#/$(svn)/;s/%/$(arch)/' $< >$@
else
ifeq ($(depends)$(dll),so)
control.tmp: control.in cycript $(lib)cycript.so
	sed -e 's/&/'"$$(dpkg-query -S $$(ldd cycript $(lib)cycript.so | sed -e '/:$$/ d; s/^[ \t]*\([^ ]* => \)\?\([^ ]*\) .*/\2/' | sort -u) 2>/dev/null | sed -e 's/:.*//; /^cycript$$/ d; s/$$/,/' | sort -u | tr '\n' ' ')"'/;s/, $$//;s/#/$(svn)/;s/%/$(arch)/' $< >$@
else
control.tmp: control.in
	sed -e 's/&/$(foreach depend,$(depends),$(depend),)/;s/,$$//;s/#/$(svn)/;s/%/$(arch)/' $< >$@
endif
endif

control: control.tmp
	[[ -e control ]] && diff control control.tmp &>/dev/null || cp -pRf control.tmp control

$(deb): $(all) control
	rm -rf package
	mkdir -p package/DEBIAN
	cp -pR control package/DEBIAN
	mkdir -p package/usr/{bin,lib,sbin}
	$(restart) extra
	cp -pR $(lib)cycript.$(dll) package/usr/lib
	cp -pR cycript package/usr/bin
	#cp -pR cyrver package/usr/sbin
	dpkg-deb -b package $(deb)
endif

all: $(all)

clean::
	rm -f *.o $(lib)cycript.$(dll) $(all) Struct.hpp lex.cy.c Cycript.tab.cc Cycript.tab.hh location.hh position.hh stack.hh cyrver Cycript.yy Cycript.l control Bridge.hpp

%.yy: %.yy.in
	./Filter.sh <$< >$@ $(filters)

%.l: %.l.in
	./Filter.sh <$< >$@ $(filters)

Cycript.tab.cc Cycript.tab.hh location.hh position.hh: Cycript.yy
	bison -v --report=state $<

lex.cy.c: Cycript.l
	flex -t $< | sed -e 's/int yyl;/yy_size_t yyl;/;s/int yyleng_r;/yy_size_t yyleng_r;/' >$@

#Parser.hpp: Parser.py Parser.dat
#	./Parser.py <Parser.dat >$@

Cycript.tab.o: Cycript.tab.cc $(header)
	$(target)$(gcc) $(flags) -c -o $@ $<

lex.cy.o: lex.cy.c $(header)
	$(target)$(gcc) $(flags) -c -o $@ $<

%.o: %.cpp $(header)
	$(target)$(gcc) $(flags) -c -o $@ $<

#objc := -x c++
%.o: %.mm $(header)
	$(target)$(gcc) $(objc) $(flags) -c -o $@ $<

$(lib)cycript.$(dll): $(code)
	$(target)$(gcc) $(flags) -shared -dynamiclib -o $@ $(filter %.o,$^) $(library) $(link)
	$(ldid) $@

cycript: Console.o $(lib)cycript.$(dll) $(inject)
	$(target)$(gcc) $(flags) -o $@ $(filter %.o,$^) -L. -lcycript $(console) $(link)
	$(entitle) cycript

package: $(deb)

test: $(deb)
	dpkg -i $(deb)
	if [[ -e target.cy ]]; then cycript -c target.cy && echo; fi
	if [[ -e jquery.js ]]; then /usr/bin/time cycript -c jquery.js >jquery.cyc.js; gzip -9c jquery.cyc.js >jquery.cyc.js.gz; wc -c jquery.{mam,gcc,cyc,bak,yui}.js; wc -c jquery.{cyc,gcc,bak,mam,yui}.js.gz; fi
	if [[ -e test.cy ]]; then cycript test.cy; fi

install: cycript $(lib)cycript.$(dll)
	cp -p cycript /usr/bin
	cp -p $(lib)cycript.$(dll) /usr/lib

.PHONY: all clean extra package control.tmp
