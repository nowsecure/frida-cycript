package:

name := Cyrver
flags := -framework CFNetwork -framework JavaScriptCore -framework WebCore -install_name /usr/lib/libcyrver.dylib
base := $(shell cd ~; pwd)/menes/tweaks
include $(base)/tweak.mk

all: cyrver

extra:
	mkdir -p package/usr/{bin,lib}
	mv package/Library/MobileSubstrate/DynamicLibraries/Cyrver.dylib package/usr/lib/libcyrver.dylib
	ln -s /usr/lib/libcyrver.dylib package/Library/MobileSubstrate/DynamicLibraries/Cyrver.dylib
	cp -a cyrver package/usr/bin

cyrver: Application.mm Cyrver.dylib
	$(target)g++ -g0 -O2 -Wall -Werror -o $@ $(filter %.mm,$^) -framework UIKit -framework Foundation -framework CoreFoundation -lobjc Cyrver.dylib -framework JavaScriptCore -F${PKG_ROOT}/System/Library/PrivateFrameworks
	ldid -S cyrver
