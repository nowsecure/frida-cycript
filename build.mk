# Cycript - The Truly Universal Scripting Language
# Copyright (C) 2009-2016  Jay Freeman (saurik)

# GNU Affero General Public License, Version 3 {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# }}}

.SECONDARY:
.DELETE_ON_ERROR:
SHELL := /bin/bash

include codesign.mk

lipo := $(shell xcrun --sdk iphoneos -f lipo)

monotonic := $(shell git log -1 --pretty=format:%ct)
version := $(shell git describe --always --tags --dirty="+" --match="v*" | sed -e 's@-\([^-]*\)-\([^-]*\)$$@+\1.\2@;s@^v@@;s@%@~@g')

deb := cycript_$(version)_iphoneos-arm.deb
zip := cycript_$(version).zip

cycript := 
cycript += Cycript.lib/cycript-apl
cycript += Cycript.lib/cycript-a32
cycript += Cycript.lib/libcycript.so
cycript += Cycript.lib/cycript0.9
cycript += Cycript.lib/libcycript.dylib
cycript += Cycript.lib/libcycript-sys.dylib
cycript += Cycript.lib/libcycript-sim.dylib
cycript += Cycript.lib/libcycript.cy
cycript += Cycript.lib/libcycript.db
cycript += Cycript.lib/libcycript.jar
cycript += Cycript.lib/libJavaScriptCore.so
cycript += Cycript.lib/l/linux
cycript += Cycript.lib/u/unknown

framework := 
framework += Cycript
framework += Headers/Cycript.h

framework := $(foreach os,ios osx,$(foreach file,$(framework),Cycript.$(os)/Cycript.framework/$(file)))

links := 
links += Cycript.lib/cynject
links += Cycript.lib/libcycript.cy
links += Cycript.lib/libsubstrate.dylib
links += Cycript.lib/cycript0.9

data := 
data += Cycript.lib/libcycript.jar
data += Cycript.lib/libcycript.db
data += Cycript.lib/libcycript.cy

local := $(data)
local += Cycript.lib/cycript-apl
local += Cycript.lib/libcycript.dylib
local += Cycript.lib/libcycript-sys.dylib
local += Cycript.lib/libcycript-sim.dylib

android := $(data)
android += Cycript.lib/cycript-a32
android += Cycript.lib/cycript-pie
android += Cycript.lib/libcycript.so
android += Cycript.lib/libJavaScriptCore.so
android += Cycript.lib/l/linux
android += Cycript.lib/u/unknown

all := cycript $(cycript) $(framework)
all: $(all)

$(zip): $(all)
	rm -f $@
	zip -r9y $@ cycript Cycript.lib Cycript.{ios,osx} $(patsubst %,--exclude %,$(links))
	zip -r9 $@ $(links)

zip: $(zip)
	ln -sf $< cycript.zip

$(deb): Cycript.lib/cycript-apl Cycript.lib/libcycript.dylib Cycript.lib/libcycript.db
	rm -rf package
	mkdir -p package/DEBIAN
	sed -e 's/#/$(version)/' control.in >package/DEBIAN/control
	mkdir -p package/usr/{bin,lib}
	cp -a cycript0.9 package/usr/lib/cycript0.9
	$(lipo) -extract armv6 -output package/usr/bin/cycript Cycript.lib/cycript-apl
	$(lipo) -extract armv6 -extract arm64 -output package/usr/lib/libcycript.dylib Cycript.lib/libcycript.dylib
	ln -s libcycript.dylib package/usr/lib/libcycript.0.dylib
	cp -a libcycript.cy package/usr/lib/libcycript.cy
	cp -a Cycript.lib/libcycript.jar package/usr/lib/libcycript.jar
	cp -a Cycript.lib/libcycript.db package/usr/lib/libcycript.db
	sqlite3 package/usr/lib/libcycript.db "delete from cache where system & $$(($$(cat build.ios-arm{v6,64}/Makefile | sed -e '/^CY_SYSTEM = \([0-9]*\)$$/{s//\1/;p;};d;' | tr $$'\n' '|') 0)) == 0; vacuum full;"
	./dpkg-deb.sh -Zlzma -b package $@

deb: $(deb)
	ln -sf $< cycript.deb

clean := 

db := 

library := libffi libuv

# make stubbornly refuses to believe that these @'s are bugs
# http://osdir.com/ml/help-make-gnu/2012-04/msg00008.html

define build_lar
$(1).a: $(1).$(2)/.libs/$(1).a
endef

define build_any
.PHONY: build-$(1)-$(2)
build-$(1)-$(2):
	$$(MAKE) -C build.$(1)-$(2)
build.$(1)-$(2)/.libs/libcycript.a: build-$(1)-$(2)
	@
clean-$(1)-$(2):
	$$(MAKE) -C build.$(1)-$(2) clean
clean += clean-$(1)-$(2)
db += build.$(1)-$(2)/libcycript.db
build.$(1)-$(2)/libcycript.db: build-$(1)-$(2)
	@
ifneq ($(1),sim)
$(foreach lib,$(library),
$(call build_lar,$(lib),$(2))
)
endif
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
build.osx-$(1)/libcycript.jar: build-osx-$(1)
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

define build_and
.PHONY: build-and-$(1)
build-and-$(1):
	$$(MAKE) -C build.and-$(1)
clean-and-$(1):
	$$(MAKE) -C build.and-$(1) clean
clean += clean-and-$(1)
db += build.and-$(1)/libcycript.db
build.and-$(1)/.libs/cycript: build-and-$(1)
	@
build.and-$(1)/cycript-pie: build-and-$(1)
	@
build.and-$(1)/.libs/libcycript.so: build-and-$(1)
	@
build.and-$(1)/libcycript.db: build-and-$(1)
	@
endef

$(foreach arch,armeabi,$(eval $(call build_and,$(arch))))

clean += $(patsubst %,%.a,$(library))
$(patsubst %,%.a,$(library)):
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^

clean: $(clean)
	rm -rf cycript Cycript.lib libcycript*.o

Cycript.lib/libcycript.dylib: build.osx-i386/.libs/libcycript.dylib build.osx-x86_64/.libs/libcycript.dylib build.ios-armv6/.libs/libcycript.dylib build.ios-arm64/.libs/libcycript.dylib
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	install_name_tool -change /System/Library/{,Private}Frameworks/JavaScriptCore.framework/JavaScriptCore $@
	codesign -s $(codesign) $@

Cycript.lib/libcycript.so: build.and-armeabi/.libs/libcycript.so
	@mkdir -p $(dir $@)
	cp -af $< $@

Cycript.lib/libJavaScriptCore.so: android/armeabi/libJavaScriptCore.so
	@mkdir -p $(dir $@)
	cp -af $< $@

Cycript.lib/%: terminfo/%
	@mkdir -p $(dir $@)
	cp -af $< $@

Cycript.lib/cycript-pie: build.and-armeabi/cycript-pie
	@mkdir -p $(dir $@)
	cp -af $< $@

%_: %
	@cp -af $< $@
	install_name_tool -change /System/Library/{,Private}Frameworks/JavaScriptCore.framework/JavaScriptCore $@
	codesign -s $(codesign) --entitlement cycript-$(word 2,$(subst ., ,$(subst -, ,$*))).xml $@

Cycript.lib/cycript-apl: build.osx-i386/.libs/cycript_ build.osx-x86_64/.libs/cycript_ build.ios-armv6/.libs/cycript_
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^

Cycript.lib/cycript-a32: build.and-armeabi/.libs/cycript
	@mkdir -p $(dir $@)
	cp -af $< $@

Cycrit.lib/libcycript.so: build.and-armeabi/.libs/libcycript.so
	@mkdir -p $(dir $@)
	cp -af $< $@

Cycript.lib/libcycript-sys.dylib:
	@mkdir -p $(dir $@)
	ln -sf libcycript.dylib $@

Cycript.lib/libcycript-sim.dylib: build.sim-i386/.libs/libcycript.dylib build.sim-x86_64/.libs/libcycript.dylib
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	codesign -s $(codesign) $@

libcycript-%.o: build.%/.libs/libcycript.a $(patsubst %,%.a,$(library)) xcode.map
	@mkdir -p $(dir $@)
	ld -r -arch $$($(lipo) -detailed_info $< | sed -e '/^Non-fat file: / ! d; s/.*: //') -o $@ -all_load -exported_symbols_list xcode.map -x $(filter %.a,$^)

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

Cycript.lib/libcycript.cy:
	@mkdir -p $(dir $@)
	ln -sf ../libcycript.cy $@

Cycript.lib/libcycript.db: $(db)
	@mkdir -p $(dir $@)
	./libcycript.py 0 $@ $(dir $(abspath $(lastword $(MAKEFILE_LIST)))) $^ </dev/null

Cycript.lib/libcycript.jar: build.osx-x86_64/libcycript.jar
	@mkdir -p $(dir $@)
	cp -af $< $@

Cycript.lib/cycript0.9:
	@mkdir -p $(dir $@)
	ln -sf ../cycript0.9 $@

cycript: cycript.in
	cp -af $< $@
	chmod 755 $@

debug: $(local)
	DYLD_LIBRARY_PATH=Cycript.lib lldb Cycript.lib/cycript-apl

install: $(local)
	sudo cp -af $(filter-out %.dylib,$^) /usr/bin
	sudo cp -af $(filter %.dylib,$^) /usr/lib

push: cycript $(android)
	adb push cycript /data/local/tmp/cycript
	adb shell mkdir -p /data/local/tmp/cycript/Cycript.lib/{l,u}
	for x in $(android); do adb push $$x /data/local/tmp/$$x; done

cast: $(zip)
	appcast.sh cycript/mac $(monotonic) $(version) $< "$(CYCRIPT_CHANGES)"

.PHONY: all cast clean debug install zip
