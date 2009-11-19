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
dll := so
apr := -lapr-1
library := 
console := $(apr) -lreadline
depends :=

restart ?= $(MAKE)
uname_s ?= $(shell uname -s)
uname_p ?= $(shell uname -p)

-include $(uname_s).mk
-include $(uname_s)-$(uname_p).mk

ifdef CY_EXECUTE
ifeq ($(filter ObjectiveC,$(filters)),)
ifneq ($(shell which gnustep-config 2>/dev/null),)
include GNUstep.mk
endif
endif
endif

flags += -Wall -Werror -Wno-parentheses #-Wno-unused
flags += -fPIC -fno-common
flags += -I. -Iinclude -I$(shell apr-1-config --includedir)

all += libcycript.$(dll)

ifdef arch
deb := $(shell grep ^Package: control.in | cut -d ' ' -f 2-)_$(shell grep ^Version: control.in | cut -d ' ' -f 2 | sed -e 's/\#/$(svn)/')_$(arch).deb

all: $(deb)

extra::

ifeq ($(depends)$(dll),dylib)
control.tmp: control.in cycript libcycript.dylib
	sed -e 's/&/'"$$(dpkg-query -S $$(otool -lah cycript *.dylib | grep dylib | grep -v ':$$' | sed -e 's/^ *name //;s/ (offset [0-9]*)$$//' | sort -u) 2>/dev/null | sed -e 's/:.*//; /^cycript$$/ d; s/$$/,/' | sort -u | tr '\n' ' ')"'/;s/, $$//;s/#/$(svn)/;s/%/$(arch)/' $< >$@
else
ifeq ($(depends)$(dll),so)
control.tmp: control.in cycript libcycript.so
	sed -e 's/&/'"$$(dpkg-query -S $$(ldd cycript libcycript.so | sed -e '/:$$/ d; s/^[ \t]*\([^ ]* => \)\?\([^ ]*\) .*/\2/' | sort -u) 2>/dev/null | sed -e 's/:.*//; /^cycript$$/ d; s/$$/,/' | sort -u | tr '\n' ' ')"'/;s/, $$//;s/#/$(svn)/;s/%/$(arch)/' $< >$@
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
	cp -pR libcycript.$(dll) package/usr/lib
	cp -pR cycript package/usr/bin
	#cp -pR cyrver package/usr/sbin
	dpkg-deb -b package $(deb)
endif

all: $(all)

clean:
	rm -f *.o libcycript.$(dll) $(all) Struct.hpp lex.cy.c Cycript.tab.cc Cycript.tab.hh location.hh position.hh stack.hh cyrver Cycript.y Cycript.l control

%.y: %.y.in
	./Filter.sh <$< >$@ $(filters)

%.l: %.l.in
	./Filter.sh <$< >$@ $(filters)

Cycript.tab.cc Cycript.tab.hh location.hh position.hh: Cycript.y
	bison -v --report=state $<

lex.cy.c: Cycript.l
	flex $<

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

libcycript.$(dll): $(code)
	$(target)$(gcc) $(flags) -shared -dynamiclib -o $@ $(filter %.o,$^) $(library) $(link)
	$(ldid) $@

cycript: Console.o libcycript.$(dll) $(inject)
	$(target)$(gcc) $(flags) -o $@ $(filter %.o,$^) -L. -lcycript $(console) $(link)
	$(entitle) cycript

package: $(deb)

test: $(deb)
	dpkg -i $(deb)
	if [[ -e target.cy ]]; then cycript -c target.cy && echo; fi
	if [[ -e jquery.js ]]; then /usr/bin/time cycript -c jquery.js >jquery.cyc.js; gzip -9c jquery.cyc.js >jquery.cyc.js.gz; wc -c jquery.{mam,gcc,cyc,bak,yui}.js; wc -c jquery.{cyc,gcc,bak,mam,yui}.js.gz; fi
	if [[ -e test.cy ]]; then cycript test.cy; fi

.PHONY: all clean extra package control.tmp
