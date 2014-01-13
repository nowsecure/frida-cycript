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

version := $(shell git describe --always --tags --dirty="+" --match="v*" | sed -e 's@-\([^-]*\)-\([^-]*\)$$@+\1.\2@;s@^v@@;s@%@~@g')
deb := cycript_$(version)_iphoneos-arm.deb

cycript := 
cycript += Cycript_/cycript
cycript += Cycript_/libcycript.dylib
cycript += Cycript_/libcycript-any.dylib
cycript += Cycript_/libcycript-sys.dylib
cycript += Cycript_/libcycript-sim.dylib

framework := 
framework += Cycript.framework/Cycript
framework += Cycript.framework/Headers/Cycript.h

all: cycript $(cycript) $(framework)

cycript.zip: all
	rm -f $@
	zip -r9y $@ cycript Cycript_ Cycript.framework

package: cycript.zip

$(deb): Cycript_/cycript Cycript_/libcycript.dylib
	rm -rf package
	mkdir -p package/DEBIAN
	sed -e 's/#/$(version)/' control.in >package/DEBIAN/control
	mkdir -p package/usr/{bin,lib}
	$(lipo) -extract armv6 -output package/usr/bin/cycript Cycript_/cycript
	$(lipo) -extract armv6 -extract arm64 -output package/usr/lib/libcycript.dylib Cycript_/libcycript.dylib
	ln -s libcycript.dylib package/usr/lib/libcycript.0.dylib
	dpkg-deb -Zlzma -b package $@

deb: $(deb)
	ln -sf $< cycript.deb

clean:
	rm -rf cycript Cycript_ libcycript*.o

# make stubbornly refuses to believe that these @'s are bugs
# http://osdir.com/ml/help-make-gnu/2012-04/msg00008.html

define build_mac
.PHONY: build-mac-$(1)
build-mac-$(1):
	$(MAKE) -C build.mac-$(1)
build.mac-$(1)/.libs/cycript: build-mac-$(1)
	@
build.mac-$(1)/.libs/libcycript.dylib: build-mac-$(1)
	@
build.mac-$(1)/.libs/libcycript-any.dylib: build-mac-$(1)
	@
endef

$(foreach arch,i386 x86_64,$(eval $(call build_mac,$(arch))))

define build_ios
.PHONY: build-ios-$(1)
build-ios-$(1):
	$(MAKE) -C build.ios-$(1)
build.ios-$(1)/.libs/libcycript.a: build-ios-$(1)
	@
endef

$(foreach arch,armv6 armv7 armv7s arm64,$(eval $(call build_ios,$(arch))))

define build_sim
.PHONY: build-sim-$(1)
build-sim-$(1):
	$(MAKE) -C build.sim-$(1)
build.sim-$(1)/.libs/libcycript.dylib: build-sim-$(1)
	@
build.sim-$(1)/.libs/libcycript.a: build-sim-$(1)
	@
endef

$(foreach arch,i386 x86_64,$(eval $(call build_sim,$(arch))))

define build_arm
build.ios-$(1)/.libs/cycript: build-ios-$(1)
	@
endef

$(foreach arch,armv6,$(eval $(call build_arm,$(arch))))

define build_arm
build.ios-$(1)/.libs/libcycript.dylib: build-ios-$(1)
	@
endef

$(foreach arch,armv6 arm64,$(eval $(call build_arm,$(arch))))

Cycript_/%.dylib: build.mac-i386/.libs/%.dylib build.mac-x86_64/.libs/%.dylib build.ios-armv6/.libs/%.dylib build.ios-arm64/.libs/%.dylib
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	install_name_tool -change /System/Library/{,Private}Frameworks/JavaScriptCore.framework/JavaScriptCore $@
	codesign -s $(codesign) $@

%_: %
	@cp -af $< $@
	install_name_tool -change /System/Library/{,Private}Frameworks/JavaScriptCore.framework/JavaScriptCore $@
	codesign -s $(codesign) --entitlement cycript-$(word 2,$(subst ., ,$(subst -, ,$*))).xml $@

Cycript_/%: build.mac-i386/.libs/%_ build.mac-x86_64/.libs/%_ build.ios-armv6/.libs/%_
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^

Cycript_/libcycript-sys.dylib:
	@mkdir -p $(dir $@)
	ln -sf libcycript.dylib $@

Cycript_/libcycript-sim.dylib: build.sim-i386/.libs/libcycript.dylib build.sim-x86_64/.libs/libcycript.dylib
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	codesign -s $(codesign) $@

libcycript-%.o: build.%/.libs/libcycript.a xcode.map
	@mkdir -p $(dir $@)
	ld -r -arch $$($(lipo) -detailed_info $< | sed -e '/^Non-fat file: / ! d; s/.*: //') -o $@ -all_load -exported_symbols_list xcode.map $< libffi.a

libcycript.o: libcycript-ios-armv6.o libcycript-ios-armv7.o libcycript-ios-armv7s.o libcycript-ios-arm64.o libcycript-sim-i386.o libcycript-sim-x86_64.o
	$(lipo) -create -output $@ $^

Cycript.framework/Cycript: libcycript.o
	@mkdir -p $(dir $@)
	cp -a $< $@

Cycript.framework/Headers/Cycript.h: Cycript.h
	@mkdir -p $(dir $@)
	cp -a $< $@

cycript: cycript.in
	cp -af $< $@
	chmod 755 $@

.PHONY: all clean package
