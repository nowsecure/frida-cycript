ifndef PKG_TARG
target :=
else
target := $(PKG_TARG)-
endif

package:

name := Cyrver
flags := -framework CFNetwork -framework JavaScriptCore -framework WebCore -install_name /usr/lib/libcyrver.dylib -I.
menes := $(shell cd ~; pwd)/menes

link := -framework CoreFoundation -framework Foundation -F${PKG_ROOT}/System/Library/PrivateFrameworks -L$(menes)/mobilesubstrate -lsubstrate -lapr-1 -lffi

all: $(name).dylib $(control)

clean:
	rm -f $(name).dylib

$(name).dylib: Tweak.mm makefile $(menes)/mobilesubstrate/substrate.h sig/*.[ch]pp
	$(target)g++ -save-temps -dynamiclib -mthumb -g0 -O2 -Wall -Werror -o $@ $(filter %.cpp,$^) $(filter %.mm,$^) -lobjc -I$(menes)/mobilesubstrate $(link) $(flags)
	ldid -S $@

package: all
	rm -rf package
	mkdir -p package/DEBIAN
	mkdir -p package/Library/MobileSubstrate/DynamicLibraries
	cp -a control $(control) package/DEBIAN
	if [[ -e Settings.plist ]]; then \
	    mkdir -p package/Library/PreferenceLoader/Preferences; \
	    cp -a Settings.png package/Library/PreferenceLoader/Preferences/$(name)Icon.png; \
	    cp -a Settings.plist package/Library/PreferenceLoader/Preferences/$(name).plist; \
	fi
	if [[ -e Tweak.plist ]]; then cp -a Tweak.plist package/Library/MobileSubstrate/DynamicLibraries/$(name).plist; fi
	cp -a $(name).dylib package/Library/MobileSubstrate/DynamicLibraries
	$(MAKE) extra
	dpkg-deb -b package $(shell grep ^Package: control | cut -d ' ' -f 2-)_$(shell grep ^Version: control | cut -d ' ' -f 2)_iphoneos-arm.deb

extra:

%: %.mm
	$(target)g++ -o $@ -Wall -Werror $< -lobjc -framework CoreFoundation -framework Foundation

.PHONY: all clean extra package

all: cyrver

extra:
	mkdir -p package/usr/{bin,lib}
	mv package/Library/MobileSubstrate/DynamicLibraries/Cyrver.dylib package/usr/lib/libcyrver.dylib
	ln -s /usr/lib/libcyrver.dylib package/Library/MobileSubstrate/DynamicLibraries/Cyrver.dylib
	cp -a cyrver package/usr/bin

cyrver: Application.mm Cyrver.dylib
	$(target)g++ -g0 -O2 -Wall -Werror -o $@ $(filter %.mm,$^) -framework UIKit -framework Foundation -framework CoreFoundation -lobjc Cyrver.dylib -framework JavaScriptCore -F${PKG_ROOT}/System/Library/PrivateFrameworks
	ldid -S cyrver
