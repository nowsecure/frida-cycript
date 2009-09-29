ifndef PKG_TARG
target :=
else
target := $(PKG_TARG)-
endif

package:

flags := -framework CFNetwork -framework JavaScriptCore -framework WebCore -install_name /usr/lib/libcycript.dylib -I.
menes := $(shell cd ~; pwd)/menes

link := -framework CoreFoundation -framework Foundation -F${PKG_ROOT}/System/Library/PrivateFrameworks -L$(menes)/mobilesubstrate -lsubstrate -lapr-1 -lffi

all: cycript libcycript.dylib libcycript.plist

clean:
	rm -f libcycript.dylib cycript libcycript.plist Struct.hpp lex.cy.c Cycript.tab.c Cycript.tab.h

libcycript.plist: Bridge.def makefile
	sed -e 's/^C/0/;s/^F/1/;s/^V/2/' Bridge.def | while read -r line; do \
	    if [[ $$line == '' ]]; then \
	        continue; \
	    fi; \
	    set $$line; \
	    if [[ $$1 =~ [#fl] ]]; then \
	        continue; \
	    fi; \
	    echo "$$2 = ($$1, \"$$3\");";  \
	done >$@

Cycript.tab.c Cycript.tab.h: Cycript.y makefile
	bison -v $<

lex.cy.c: Cycript.l
	flex $<

Struct.hpp:
	$$($(target)gcc -print-prog-name=cc1obj) -print-objc-runtime-info </dev/null >$@

#Parser.hpp: Parser.py Parser.dat
#	./Parser.py <Parser.dat >$@

libcycript.dylib: Library.mm makefile $(menes)/mobilesubstrate/substrate.h sig/*.[ch]pp Struct.hpp Parser.hpp lex.cy.c Cycript.tab.c Cycript.tab.h
	$(target)g++ -dynamiclib -mthumb -g0 -O2 -Wall -Werror -o $@ $(filter %.cpp,$^) $(filter %.c,$^) $(filter %.mm,$^) -lobjc -I$(menes)/mobilesubstrate $(link) $(flags) -DYYDEBUG=1
	ldid -S $@

cycript: Application.mm libcycript.dylib
	$(target)g++ -g0 -O2 -Wall -Werror -o $@ $(filter %.mm,$^) -framework UIKit -framework Foundation -framework CoreFoundation -lobjc libcycript.dylib
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
	cycript

.PHONY: all clean extra package
