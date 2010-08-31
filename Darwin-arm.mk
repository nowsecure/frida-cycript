flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += #cyrver

arch := iphoneos-arm
console += -framework UIKit
depends += apr-lib readline libffi mobilesubstrate
#library += -framework CFNetwork
library += -framework WebCore
# XXX: all Darwin, maybe all device, should have this
library += -lsubstrate

ldid := ldid -S
entitle := ldid -Scycript.xml

cyrver: Server.o
	$(target)g++ $(flags) -o $@ $(filter %.o,$^) \
	    -lapr-1 -lsubstrate -framework CFNetwork
	$(ldid) $@

extra::
	sed -i -e '/^Depends: / s/\<mobilesubstrate\>/mobilesubstrate (>= 0.9.3072)/g' package/DEBIAN/control
	#mkdir -p package/System/Library/LaunchDaemons
	#cp -pR com.saurik.Cyrver.plist package/System/Library/LaunchDaemons
