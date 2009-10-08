ifndef PKG_TARG
target :=
else
target := $(PKG_TARG)-
endif

package:

flags := -mthumb -g3 -O0 -Wall -Werror -I. -fno-common
flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all: cycript libcycript.dylib libcycript.plist

clean:
	rm -f *.o libcycript.dylib cycript libcycript.plist Struct.hpp lex.cy.c Cycript.tab.cc Cycript.tab.hh location.hh position.hh stack.hh

libcycript.plist: Bridge.def
	{ \
	    sed -e 's/^C/0/;s/^F/1/;s/^V/2/' Bridge.def | while read -r line; do \
	        if [[ $$line == '' ]]; then \
	            continue; \
	        fi; \
	        set $$line; \
	        if [[ $$1 =~ [#fl:] ]]; then \
	            continue; \
	        fi; \
	        echo "$$2 = ($$1, \"$$3\");";  \
	    done; \
	    grep ^: Bridge.def | sed -e 's/^: \([^ ]*\) \(.*\)/":\1" = "\2";/'; \
	} >$@

Cycript.tab.cc Cycript.tab.hh location.hh position.hh: Cycript.y
	bison -v --report=state $<

lex.cy.c: Cycript.l
	flex $<

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

#Parser.hpp: Parser.py Parser.dat
#	./Parser.py <Parser.dat >$@

%.o: sig/%.cpp
	$(target)g++ $(flags) -c -o $@ $<

Cycript.tab.o: Cycript.tab.cc Cycript.tab.hh Parser.hpp Pooling.hpp
	$(target)g++ $(flags) -c -o $@ $<

lex.cy.o: lex.cy.c Cycript.tab.hh Parser.hpp Pooling.hpp
	$(target)g++ $(flags) -c -o $@ $<

Output.o: Output.cpp Parser.hpp Pooling.hpp
	$(target)g++ $(flags) -c -o $@ $<

Library.o: Library.mm Cycript.tab.hh Parser.hpp Pooling.hpp Struct.hpp cycript.hpp
	$(target)g++ $(flags) -c -o $@ $<

Application.o: Application.mm Cycript.tab.hh Parser.hpp Pooling.hpp cycript.hpp
	$(target)g++ $(flags) -c -o $@ $<

libcycript.dylib: ffi_type.o parse.o Output.o Cycript.tab.o lex.cy.o Library.o
	$(target)g++ $(flags) -dynamiclib -o $@ $(filter %.o,$^) -lobjc -framework CFNetwork -framework JavaScriptCore -framework WebCore -install_name /usr/lib/libcycript.dylib -framework CoreFoundation -framework Foundation -L$(menes)/mobilesubstrate -lsubstrate -lapr-1 -lffi -framework UIKit
	ldid -S $@

cycript: Application.o libcycript.dylib
	$(target)g++ $(flags) -o $@ $(filter %.o,$^) -framework UIKit -framework Foundation -framework CoreFoundation -lobjc libcycript.dylib -lreadline -framework JavaScriptCore
	ldid -S cycript

package: all
	rm -rf package
	mkdir -p package/DEBIAN
	cp -a control package/DEBIAN
	mkdir -p package/Library/MobileSubstrate/DynamicLibraries
	if [[ -e Settings.plist ]]; then \
	    mkdir -p package/Library/PreferenceLoader/Preferences; \
	    cp -a Settings.png package/Library/PreferenceLoader/Preferences/CycriptIcon.png; \
	    cp -a Settings.plist package/Library/PreferenceLoader/Preferences/Cycript.plist; \
	fi
	if [[ -e Tweak.plist ]]; then cp -a Tweak.plist package/Library/MobileSubstrate/DynamicLibraries/Cycript.plist; fi
	#cp -a Cycript.dylib package/Library/MobileSubstrate/DynamicLibraries
	mkdir -p package/usr/{bin,lib}
	cp -a libcycript.dylib package/usr/lib
	#ln -s /usr/lib/libcycript.dylib package/Library/MobileSubstrate/DynamicLibraries/Cycript.dylib
	cp -a cycript package/usr/bin
	cp -a libcycript.plist package/usr/lib
	dpkg-deb -b package $(shell grep ^Package: control | cut -d ' ' -f 2-)_$(shell grep ^Version: control | cut -d ' ' -f 2)_iphoneos-arm.deb

test: package
	dpkg -i $(shell grep ^Package: control | cut -d ' ' -f 2-)_$(shell grep ^Version: control | cut -d ' ' -f 2)_iphoneos-arm.deb
	cycript /Applications/HelloCycript.app/HelloCycript

.PHONY: all clean extra package
