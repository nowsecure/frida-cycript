binary := Cycript_/cycript

$(deb): $(binary) $(patsubst %,Cycript_/libcycript%dylib,. -sim. -sys.) control
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
