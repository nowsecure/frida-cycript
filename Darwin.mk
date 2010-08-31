dll := dylib
link += -lobjc -framework CoreFoundation
console += -framework Foundation
library += -install_name $(prefix)/lib/libcycript.$(dll)
library += -framework Foundation
console += -framework JavaScriptCore
# XXX: do I just need WebCore?
console += -framework WebKit
library += -undefined dynamic_lookup
library += -liconv
flags += -I/usr/include/ffi
apr_config := /usr/bin/apr-1-config
flags += -arch i386 -arch x86_64 #-arch armv6

prefix := /sw

flags += -DCY_ATTACH -DCY_LIBRARY='"$(prefix)/lib/libcycript.dylib"'
code += Handler.o
inject += Mach/Inject.o
Mach/Inject.o: Trampoline.t.hpp Baton.hpp

%.t.hpp: %.t.cpp trampoline.sh Baton.hpp Trampoline.hpp Darwin.mk
	./trampoline.sh $@ $*.t.dylib $* sed $(target){otool,lipo,nm,gcc} $(flags) -dynamiclib -g0 -fno-stack-protector -fno-exceptions -Iinclude $< -o $*.t.dylib

clean::
	rm -f Trampoline.t.hpp

include Execute.mk
include ObjectiveC.mk
