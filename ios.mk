srcdir := .

sed := sed
git := git

arch := iphoneos-arm

#ifneq ($(git),)
version := $(shell $(git) describe --always --tags --dirty="+" --match="v*" | $(sed) -e 's@-\([^-]*\)-\([^-]*\)$$@+\1.\2@;s@^v@@;s@%@~@g')
#else
#version := @PACKAGE_VERSION@
#endif

deb := $(shell grep ^Package: $(srcdir)/control.in | cut -d ' ' -f 2-)_$(shell grep ^Version: $(srcdir)/control.in | cut -d ' ' -f 2 | $(sed) -e 's/\#/$(version)/')_$(arch).deb

binary := Cycript_/cycript

$(deb): $(binary) $(patsubst %,Cycript_/libcycript%dylib,. -any. -sim. -sys.) control
	rm -rf package
	mkdir -p package/DEBIAN
	cp -pR control package/DEBIAN
	mkdir -p package/usr/{bin,lib}
	cp -pR $(filter %.dylib,$^) package/usr/lib
	cp -pR $< package/usr/bin
	dpkg-deb -b package $(deb)

control: control.tmp
	[[ -e control ]] && diff control control.tmp &>/dev/null || cp -pRf control.tmp control

# XXX: this is now all broken
depends := apr-lib, readline, libffi (>= 1:3.0.10-5), adv-cmds
ifeq ($(depends)$(dll),dylib)
control.tmp: control.in $(binary) .libs/$(lib)cycript.dylib
	$(sed) -e 's/&/'"$$(dpkg-query -S $$(otool -lah $(binary) .libs/*.dylib | grep dylib | grep -v ':$$' | $(sed) -e 's/^ *name //;s/ (offset [0-9]*)$$//' | sort -u) 2>/dev/null | $(sed) -e 's/:.*//; /^cycript$$/ d; s/$$/,/' | sort -u | tr '\n' ' ')"'/;s/, $$//;s/#/$(version)/;s/%/$(arch)/' $< >$@
else
ifeq ($(depends)$(dll),so)
control.tmp: control.in $(binary) .libs/$(lib)cycript.so
	$(sed) -e 's/&/'"$$(dpkg-query -S $$(ldd $(binary) $(lib)cycript.so | $(sed) -e '/:$$/ d; s/^[ \t]*\([^ ]* => \)\?\([^ ]*\) .*/\2/' | sort -u) 2>/dev/null | $(sed) -e 's/:.*//; /^cycript$$/ d; s/$$/,/' | sort -u | tr '\n' ' ')"'/;s/, $$//;s/#/$(version)/;s/%/$(arch)/' $< >$@
else
control.tmp: control.in
	$(sed) -e 's/&/$(depends)/;s/,$$//;s/#/$(version)/;s/%/$(arch)/' $< >$@
endif
endif

clean::
	rm -rf control

.PHONY: clean
