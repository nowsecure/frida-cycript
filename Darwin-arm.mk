flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += #cyrver

arch := iphoneos-arm
console += -framework UIKit
depends += readline libffi mobilesubstrate sqlite3-lib

ldid := ldid -S
entitle := ldid -Scycript.xml

cyrver: Server.o
	$(target)g++ $(flags) -o $@ $(filter %.o,$^) \
	    -lapr-1 -lsubstrate -framework CFNetwork
	$(ldid) $@

extra:
	#mkdir -p package/System/Library/LaunchDaemons
	#cp -a com.saurik.Cyrver.plist package/System/Library/LaunchDaemons
