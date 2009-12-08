flags += -DCY_ATTACH
code += Handler.o
inject += Mach/Inject.o
Mach/Inject.o: Trampoline.t.hpp Baton.hpp

%.t.hpp: %.t.cpp
	$(target)gcc -c -fno-exceptions -Iinclude -o $*.t.o $< && { file=($$($(target)otool -l $*.t.o | sed -e 'x; /^1/ { x; /^ *filesize / { s/^.* //; p; }; /^ *fileoff / { s/^.* //; p; }; x; }; x; /^ *cmd LC_SEGMENT$$/ { s/.*/1/; x; }; d;')); od -t x1 -j $${file[0]} -N $${file[1]} $*.t.o | sed -e 's/^[^ ]*//' | tr $$'\n' ' ' | sed -e 's/  */ /g;s/^ *//;s/ $$//;s/ /,/g;s/\([^,][^,]\)/0x\1/g' | sed -e 's/^/static const char $*_[] = {/;s/$$/};/' && echo && echo "/*" && $(target)otool -vVt $*.t.o && echo "*/"; } >$@ && rm -f $*.t.o
