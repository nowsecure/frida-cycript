# XXX: objective-c exists on non-Darwin

dll := dylib
flags += -DCY_EXECUTE
link += -lobjc -framework CoreFoundation
console += -framework Foundation
library += -install_name /usr/lib/libcycript.$(dll)
library += -framework Foundation -framework CFNetwork
library += -framework JavaScriptCore -framework WebCore
library += -lsubstrate -liconv

flags += -DCY_ATTACH
code += Handler.o
inject += Mach/Inject.o
Mach/Inject.o: Trampoline.t.hpp Baton.hpp

%.t.hpp: %.t.cpp
	$(target)gcc -c -o $*.t.o $< && { $(target)otool -l $*.t.o | sed -e '/^ *segname __TEXT$$/ { x; s/^ *sectname //; p; }; /^ *sectname / x; d;' | while read -r sect; do $(target)otool -s __TEXT "$$sect" Trampoline.t.o; done | sed -e '/:$$/ d; / section$$/ d; s/^[^ \t]*[ \t]*//;s/ $$//;s/ /\n/g' | sed -e 's/\(..\)\(..\)\(..\)\(..\)/0\x\4,0\x\3,0\x\2,0\x\1/' | tr '\n' ',' | sed -e '$$ s/,$$//; s/^/static const char $*_[] = {/;s/$$/};\n/' && echo && echo "/*" && $(target)otool -vVt $*.t.o && echo "*/"; } >$@ && rm -f $*.t.o

include ObjectiveC.mk
