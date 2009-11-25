#!/usr/bin/env bash
shopt -s expand_aliases
unalias -a
case `uname` in
(Linux)
	libdirs=('/usr/share/gettext')
	;;
(FreeBSD)
	alias sed=gsed
	libdirs=('/usr/local/share/gettext')
	;;
esac
aclocal
sed -e 's/AC_PROG_AWK/dnl &/' -i aclocal.m4
cat `aclocal --print-ac-dir`/check_gnu_make.m4 find_apr.m4 >> aclocal.m4
autoconf
function filter()
{
	sed -e '/no proper invocation of AM_INIT_AUTOMAKE was found\./d' \
		-e '/You should verify that configure\.ac invokes AM_INIT_AUTOMAKE,/d' \
		-e '/that aclocal\.m4 is present in the top-level directory,/d' \
		-e '/and that aclocal\.m4 was recently regenerated (using aclocal)./d' \
		-e "/no \`Makefile\.am' found for any configure output/d"
}
automake -acf 2>&1 | filter
for libdir in ${libdirs[*]}; do
	automake -acf --libdir $libdir 2>&1 | filter
done
