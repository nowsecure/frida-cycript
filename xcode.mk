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

libs := 
libs += .libs/cycript
libs += .libs/libcycript.dylib
libs += .libs/libcycript-any.dylib
libs += .libs/libcycript-sys.dylib
libs += .libs/libcycript-sim.dylib
libs += .libs/libcycript.o

all: cycript $(libs)

clean:
	rm -rf cycript .libs

build-mac:
	$(MAKE) -C build.mac

build-ios:
	$(MAKE) -C build.ios

build-sim:
	$(MAKE) -C build.sim

# make stubbornly refuses to believe that these @'s are bugs
# http://osdir.com/ml/help-make-gnu/2012-04/msg00008.html

build.mac/.libs/cycript: build-mac
	@
build.mac/.libs/libcycript.dylib: build-mac
	@
build.mac/.libs/libcycript-any.dylib: build-mac
	@

build.ios/.libs/cycript: build-ios
	@
build.ios/.libs/libcycript.dylib: build-ios
	@
build.ios/.libs/libcycript-any.dylib: build-ios
	@
build.ios/.libs/libcycript.a: build-ios
	@

build.sim/.libs/libcycript.dylib: build-sim
	@
build.sim/.libs/libcycript.a: build-sim
	@

.libs/%: build.mac/.libs/% build.ios/.libs/%
	@mkdir -p .libs
	lipo -create -output $@ $^

.libs/%-ios.a: build.ios/.libs/%.a build.sim/.libs/%.a
	@mkdir -p .libs
	lipo -create -output $@ $^

.libs/libcycript-sys.dylib:
	@mkdir -p .libs
	ln -sf libcycript.dylib $@

.libs/libcycript-sim.dylib: build.sim/.libs/libcycript.dylib
	@mkdir -p .libs
	cp -af $< $@

.libs/libcycript-%.o: build.%/.libs/libcycript.a
	@mkdir -p .libs
	ld -r -arch $$(lipo -detailed_info $< | sed -e '/^Non-fat file: / ! d; s/.*: //') -o $@ -all_load $< libffi.a

.libs/libcycript.o: .libs/libcycript-ios.o .libs/libcycript-sim.o
	lipo -create -output $@ $^

cycript: cycript.in
	cp -af $< $@
	chmod 755 $@

.PHONY: all clean build-mac build-ios build-sim
