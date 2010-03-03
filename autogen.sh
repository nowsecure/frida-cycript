#!/usr/bin/env bash
set -e
shopt -s expand_aliases
unalias -a
case `uname` in
(Linux)
	libdirs=('/usr/share/gettext')
	;;
(FreeBSD)
	alias sed=gsed
	libdirs=()
	;;
esac
aclocal
sed -e 's/AC_PROG_AWK/dnl &/' -i aclocal.m4
cat `aclocal --print-ac-dir`/check_gnu_make.m4 find_apr.m4 >> aclocal.m4
function filter()
{
	sed -e '/no proper invocation of AM_INIT_AUTOMAKE was found\./d' \
		-e '/You should verify that configure\.ac invokes AM_INIT_AUTOMAKE,/d' \
		-e '/that aclocal\.m4 is present in the top-level directory,/d' \
		-e '/and that aclocal\.m4 was recently regenerated (using aclocal)\./d' \
		-e "/no \`Makefile\.am' found for any configure output/d" \
		-e "/Consider adding \`AC_CONFIG_MACRO_DIR(\[m4\])' to configure\.ac and/d" \
		-e '/rerunning libtoolize, to keep the correct libtool macros in-tree\./d' \
		-e "/Consider adding \`-I m4' to ACLOCAL_AMFLAGS in Makefile\.am\./d"
}
automake -acf 2>&1 | filter
for libdir in ${libdirs[*]}; do
	automake -acf --libdir $libdir 2>&1 | filter
done
libtoolize -ci | filter
autoconf
