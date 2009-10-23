flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += Cycript.$(dll) #cyrver

arch := iphoneos-arm
ldid := ldid -S
console += -framework UIKit
depends += readline libffi mobilesubstrate
code += Handler.o

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
	if [[ -e Settings.plist ]]; then \
	    mkdir -p package/Library/PreferenceLoader/Preferences; \
	    cp -a Settings.png package/Library/PreferenceLoader/Preferences/CycriptIcon.png; \
	    cp -a Settings.plist package/Library/PreferenceLoader/Preferences/Cycript.plist; \
	fi
	if [[ -e Tweak.plist ]]; then cp -a Tweak.plist package/Library/MobileSubstrate/DynamicLibraries/Cycript.plist; fi
	cp -a Cycript.$(dll) package/Library/MobileSubstrate/DynamicLibraries

