# Cycript - Optimizing JavaScript Compiler/Runtime
# Copyright (C) 2009-2013  Jay Freeman (saurik)

# GNU General Public License, Version 3 {{{
#
# Cycript is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# Cycript is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
# }}}

.DELETE_ON_ERROR:
SHELL := /bin/bash

include codesign.mk

lipo := $(shell xcrun --sdk iphoneos -f lipo)

monotonic := $(shell git log -1 --pretty=format:%ct)
version := $(shell git describe --always --tags --dirty="+" --match="v*" | sed -e 's@-\([^-]*\)-\([^-]*\)$$@+\1.\2@;s@^v@@;s@%@~@g')

deb := cycript_$(version)_iphoneos-arm.deb
zip := cycript_$(version).zip

cycript := 
cycript += Cycript.lib/cycript
cycript += Cycript.lib/cycript0.9
cycript += Cycript.lib/libcycript.dylib
cycript += Cycript.lib/libcycript-sys.dylib
cycript += Cycript.lib/libcycript-sim.dylib

framework := 
framework += Cycript
framework += Headers/Cycript.h

framework := $(foreach os,ios osx,$(foreach file,$(framework),Cycript.$(os)/Cycript.framework/$(file)))

links := 
links += Cycript.lib/libsubstrate.dylib
links += Cycript.lib/cycript0.9

all := cycript $(cycript) $(framework)
all: $(all)

$(zip): $(all)
	rm -f $@
	zip -r9y $@ cycript Cycript.lib Cycript.{ios,osx} $(patsubst %,--exclude %,$(links))
	zip -r9 $@ $(links)

zip: $(zip)
	ln -sf $< cycript.zip

$(deb): Cycript.lib/cycript Cycript.lib/libcycript.dylib
	rm -rf package
	mkdir -p package/DEBIAN
	sed -e 's/#/$(version)/' control.in >package/DEBIAN/control
	mkdir -p package/usr/{bin,lib}
	cp -a modules package/usr/lib/cycript0.9
	$(lipo) -extract armv6 -output package/usr/bin/cycript Cycript.lib/cycript
	$(lipo) -extract armv6 -extract arm64 -output package/usr/lib/libcycript.dylib Cycript.lib/libcycript.dylib
	ln -s libcycript.dylib package/usr/lib/libcycript.0.dylib
	dpkg-deb -Zlzma -b package $@

deb: $(deb)
	ln -sf $< cycript.deb

clean:
	rm -rf cycript Cycript.lib libcycript*.o

# make stubbornly refuses to believe that these @'s are bugs
# http://osdir.com/ml/help-make-gnu/2012-04/msg00008.html

define build_any
.PHONY: build-$(1)-$(2)
build-$(1)-$(2):
	$(MAKE) -C build.$(1)-$(2)
build.$(1)-$(2)/.libs/libcycript.a: build-$(1)-$(2)
	@
endef

define build_lib
build.$(1)-$(2)/.libs/libcycript.dylib: build-$(1)-$(2)
	@
endef

define build_osx
$(call build_any,osx,$(1))
$(call build_lib,osx,$(1))
build.osx-$(1)/.libs/cycript: build-osx-$(1)
	@
endef

$(foreach arch,i386 x86_64,$(eval $(call build_osx,$(arch))))

define build_ios
$(call build_any,ios,$(1))
endef

$(foreach arch,armv6 armv7 armv7s arm64,$(eval $(call build_ios,$(arch))))

define build_sim
$(call build_any,sim,$(1))
$(call build_lib,sim,$(1))
endef

$(foreach arch,i386 x86_64,$(eval $(call build_sim,$(arch))))

define build_arm
build.ios-$(1)/.libs/cycript: build-ios-$(1)
	@
endef

$(foreach arch,armv6,$(eval $(call build_arm,$(arch))))

define build_arm
$(call build_lib,ios,$(1))
endef

$(foreach arch,armv6 arm64,$(eval $(call build_arm,$(arch))))

Cycript.lib/libcycript.dylib: build.osx-i386/.libs/libcycript.dylib build.osx-x86_64/.libs/libcycript.dylib build.ios-armv6/.libs/libcycript.dylib build.ios-arm64/.libs/libcycript.dylib
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	install_name_tool -change /System/Library/{,Private}Frameworks/JavaScriptCore.framework/JavaScriptCore $@
	codesign -s $(codesign) $@

%_: %
	@cp -af $< $@
	install_name_tool -change /System/Library/{,Private}Frameworks/JavaScriptCore.framework/JavaScriptCore $@
	codesign -s $(codesign) --entitlement cycript-$(word 2,$(subst ., ,$(subst -, ,$*))).xml $@

Cycript.lib/%: build.osx-i386/.libs/%_ build.osx-x86_64/.libs/%_ build.ios-armv6/.libs/%_
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^

Cycript.lib/libcycript-sys.dylib:
	@mkdir -p $(dir $@)
	ln -sf libcycript.dylib $@

Cycript.lib/libcycript-sim.dylib: build.sim-i386/.libs/libcycript.dylib build.sim-x86_64/.libs/libcycript.dylib
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	codesign -s $(codesign) $@

libcycript-%.o: build.%/.libs/libcycript.a xcode.map
	@mkdir -p $(dir $@)
	ld -r -arch $$($(lipo) -detailed_info $< | sed -e '/^Non-fat file: / ! d; s/.*: //') -o $@ -all_load -exported_symbols_list xcode.map $< libffi.a

libcycript-ios.o: libcycript-ios-armv6.o libcycript-ios-armv7.o libcycript-ios-armv7s.o libcycript-ios-arm64.o libcycript-sim-i386.o libcycript-sim-x86_64.o
	$(lipo) -create -output $@ $^

libcycript-osx.o: libcycript-osx-i386.o libcycript-osx-x86_64.o
	$(lipo) -create -output $@ $^

Cycript.%/Cycript.framework/Cycript: libcycript-%.o
	@mkdir -p $(dir $@)
	cp -a $< $@

Cycript.%/Cycript.framework/Headers/Cycript.h: Cycript.h
	@mkdir -p $(dir $@)
	cp -a $< $@

Cycript.lib/cycript0.9:
	@mkdir -p $(dir $@)
	ln -s ../modules $@

cycript: cycript.in
	cp -af $< $@
	chmod 755 $@

install: Cycript.lib/cycript Cycript.lib/libcycript.dylib Cycript.lib/libcycript-sys.dylib Cycript.lib/libcycript-sim.dylib
	sudo cp -af $(filter-out %.dylib,$^) /usr/bin
	sudo cp -af $(filter %.dylib,$^) /usr/lib


cast: $(zip)
	appcast.sh cycript/mac $(monotonic) $(version) $< "$(CYCRIPT_CHANGES)"

.PHONY: all cast clean install zip
