flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += Cycript.$(dll) #cyrver

arch := iphoneos-arm
console += -framework UIKit
depends += readline libffi mobilesubstrate sqlite3-lib
code += Handler.o
inject += Mach/Inject.o

Mach/Inject.o: Trampoline.t.hpp Baton.hpp

%.t.hpp: %.t.cpp
	$(target)gcc -c -o $*.t.o $< && $(target)otool -s __TEXT __text $*.t.o | tail -n +3 | sed -e 's/^[^ ]* //;s/ $$//;s/ /\n/g' | sed -e 's/\(..\)\(..\)\(..\)\(..\)/0\x\4,0\x\3,0\x\2,0\x\1/' | tr '\n' ',' | sed -e '$$ s/,$$//; s/^/static const char $*_[] = {/;s/$$/};\n/' >$@ && rm -f $*.t.o

ldid := ldid -S
entitle := ldid -Scycript.xml

Cycript.$(dll): Connector.o
	$(target)g++ $(flags) -dynamiclib -o $@ $(filter %.o,$^) \
	    -lobjc -lapr-1 -lsubstrate \
	    -framework CoreFoundation
	ldid -S $@

cyrver: Server.o
	$(target)g++ $(flags) -o $@ $(filter %.o,$^) \
	    -lapr-1 -lsubstrate -framework CFNetwork
	$(ldid) $@

extra:
	mkdir -p package/System/Library/LaunchDaemons
	#cp -a com.saurik.Cyrver.plist package/System/Library/LaunchDaemons
	mkdir -p package/Library/MobileSubstrate/DynamicLibraries
	cp -a Cycript.$(dll) package/Library/MobileSubstrate/DynamicLibraries

