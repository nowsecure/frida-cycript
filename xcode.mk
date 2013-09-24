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

$(foreach arch,armv6 armv7 armv7s,$(eval $(call build_ios,$(arch))))

define build_sim
.PHONY: build-sim-$(1)
build-sim-$(1):
	$(MAKE) -C build.sim-$(1)
build.sim-$(1)/.libs/libcycript.dylib: build-sim-$(1)
	@
build.sim-$(1)/.libs/libcycript.a: build-sim-$(1)
	@
endef

$(foreach arch,i386,$(eval $(call build_sim,$(arch))))

define build_arm
build.ios-$(1)/.libs/cycript: build-ios-$(1)
	@
build.ios-$(1)/.libs/libcycript.dylib: build-ios-$(1)
	@
build.ios-$(1)/.libs/libcycript-any.dylib: build-ios-$(1)
	@
endef

$(foreach arch,armv6,$(eval $(call build_arm,$(arch))))

Cycript_/%: build.mac-i386/.libs/% build.mac-x86_64/.libs/% build.ios-armv6/.libs/%
	@mkdir -p $(dir $@)
	$(lipo) -create -output $@ $^
	# XXX: this should probably not entitle the dylibs
	codesign -s $(codesign) --entitlement cycript.xml $@

Cycript_/libcycript-sys.dylib:
	@mkdir -p $(dir $@)
	ln -sf libcycript.dylib $@

Cycript_/libcycript-sim.dylib: build.sim-i386/.libs/libcycript.dylib
	@mkdir -p $(dir $@)
	cp -af $< $@
	codesign -s $(codesign) $@

libcycript-%.o: build.%/.libs/libcycript.a
	@mkdir -p $(dir $@)
	ld -r -arch $$($(lipo) -detailed_info $< | sed -e '/^Non-fat file: / ! d; s/.*: //') -o $@ -all_load $< libffi.a

libcycript.o: libcycript-ios-armv6.o libcycript-ios-armv7.o libcycript-ios-armv7s.o libcycript-sim-i386.o
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
