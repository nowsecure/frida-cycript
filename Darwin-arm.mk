flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += Cycript.$(dll) #cyrver

arch := iphoneos-arm
ldid := ldid -S
console += -framework UIKit

Cycript.$(dll): Connector.o
	$(target)g++ $(flags) -dynamiclib -o $@ $(filter %.o,$^) \
	    -lobjc -lapr-1 -lsubstrate \
	    -framework CoreFoundation
	ldid -S $@

cyrver: Server.o
	$(target)g++ $(flags) -o $@ $(filter %.o,$^) \
	    -lapr-1 -lsubstrate -framework CFNetwork
	$(ldid) $@
