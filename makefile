SHELL := $(shell which bash 2>/dev/null)

ifndef PKG_TARG
target :=
else
target := $(PKG_TARG)-
endif

flags :=
objc :=

svn := $(shell svnversion)

all:
all := libcycript.plist cycript

dpkg_architecture := $(shell which dpkg-architecture 2>/dev/null)
ifneq ($(dpkg_architecture),)
arch := $(shell $(dpkg_architecture) -qDEB_HOST_ARCH 2>/dev/null)
endif

header := Cycript.tab.hh Parser.hpp Pooling.hpp cycript.hpp
code := ffi_type.o parse.o 
code += Replace.o Output.o
code += Cycript.tab.o lex.cy.o
code += Network.o Parser.o
code += JavaScriptCore.o Library.o

filters := C
ldid := echo
dll := so
apr := $(shell apr-1-config --link-ld)
library := $(apr) -lffi -liconv
console := $(apr) -lreadline
depends :=

uname_s := $(shell uname -s)
uname_p := $(shell uname -p)
-include $(uname_s).mk
-include $(uname_s)-$(uname_p).mk

ifeq ($(filter ObjectiveC,$(filters)),)
ifneq ($(shell which gnustep-config 2>/dev/null),)
include GNUstep.mk
endif
endif

flags += -g3 -O0 -DYYDEBUG=1
#flags += -g0 -O3
flags += -Wall -Werror -Wno-parentheses
flags += -fPIC -fno-common
flags += -I. -I$(shell apr-1-config --includedir)

all += libcycript.$(dll)

ifdef arch
deb := $(shell grep ^Package: control | cut -d ' ' -f 2-)_$(shell grep ^Version: control | cut -d ' ' -f 2 | sed -e 's/\#/$(svn)/')_$(arch).deb

all: $(deb)

extra:

$(deb): $(all)
	rm -rf package
	mkdir -p package/DEBIAN
	sed -e 's/&/$(foreach depend,$(depends),$(depend),)/;s/,$$//;s/#/$(svn)/;s/%/$(arch)/' control >package/DEBIAN/control
	$(MAKE) extra
	mkdir -p package/usr/{bin,lib,sbin}
	cp -a libcycript.$(dll) package/usr/lib
	cp -a cycript package/usr/bin
	#cp -a cyrver package/usr/sbin
	cp -a libcycript.plist package/usr/lib
	dpkg-deb -b package $(deb)
endif

all: $(all)

clean:
	rm -f *.o libcycript.$(dll) cycript libcycript.plist Struct.hpp lex.cy.c Cycript.tab.cc Cycript.tab.hh location.hh position.hh stack.hh cyrver Cycript.y

libcycript.plist: Bridge.def
	{ \
	    echo '({'; \
	    grep '^[CFV]' Bridge.def | sed -e 's/^C/0/;s/^F/1/;s/^V/2/' | sed -e 's/"/\\"/g;s/^\([^ ]*\) \([^ ]*\) \(.*\)$$/\2 = (\1, \"\3\");/'; \
	    echo '},{'; \
	    grep '^:' Bridge.def | sed -e 's/^: \([^ ]*\) \(.*\)/"\1" = "\2";/'; \
	    echo '},{'; \
	    grep '^[EST]' Bridge.def | sed -e 's/^S/0/;s/^T/1/;s/^E/2/' | sed -e 's/^2\(.*\)$$/1\1 i/' | sed -e 's/"/\\"/g;s/^\([^ ]*\) \([^ ]*\) \(.*\)$$/\2 = (\1, \"\3\");/'; \
	    echo '})'; \
	} >$@

Cycript.y: Cycript.y.in
	./Filter.sh <$< >$@ $(filters)

Cycript.tab.cc Cycript.tab.hh location.hh position.hh: Cycript.y
	bison -v --report=state $<

lex.cy.c: Cycript.l
	flex $<

#Parser.hpp: Parser.py Parser.dat
#	./Parser.py <Parser.dat >$@

%.o: sig/%.cpp
	$(target)g++ $(flags) -c -o $@ $<

Cycript.tab.o: Cycript.tab.cc $(header)
	$(target)g++ $(flags) -c -o $@ $<

lex.cy.o: lex.cy.c $(header)
	$(target)g++ $(flags) -c -o $@ $<

%.o: %.cpp $(header)
	$(target)g++ $(flags) -c -o $@ $<

#objc := -x c++
%.o: %.mm $(header)
	$(target)g++ $(objc) $(flags) -c -o $@ $<

libcycript.$(dll): $(code)
	$(target)g++ $(flags) -shared -dynamiclib -o $@ $(filter %.o,$^) $(library) $(link)
	$(ldid) -S $@

cycript: Console.o libcycript.$(dll)
	$(target)g++ $(flags) -o $@ $(filter %.o,$^) -L. -lcycript $(console) $(link)
	$(ldid) -S cycript

package: $(deb)

test: $(deb)
	dpkg -i $(deb)
	cycript test.cy

.PHONY: all clean extra package
