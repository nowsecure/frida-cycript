flags += -F${PKG_ROOT}/System/Library/PrivateFrameworks

all += Cycript.$(dll) #cyrver

arch := iphoneos-arm

Cycript.$(dll): Connector.o
	$(target)g++ $(flags) -dynamiclib -o $@ $(filter %.o,$^) \
	    -lobjc -lapr-1 -lsubstrate \
	    -framework CoreFoundation
	ldid -S $@
