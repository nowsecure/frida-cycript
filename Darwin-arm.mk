flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += Cycript.$(dll) #cyrver

arch := iphoneos-arm
ldid := ldid -S
console += -framework UIKit
depends += readline libffi mobilesubstrate sqlite3-lib
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
	cp -a Cycript.$(dll) package/Library/MobileSubstrate/DynamicLibraries

