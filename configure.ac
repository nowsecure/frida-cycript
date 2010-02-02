AC_PREREQ([2.65])
AC_INIT([Cycript], [0.9], [saurik@saurik.com], [cycript], [http://www.cycript.org/])
AC_CONFIG_SRCDIR([Console.cpp])
AC_CONFIG_AUX_DIR([.])
LT_INIT([disable-static])
AX_PROG_BISON([
	BISON=`echo $YACC | $SED -e 's/ -y//'`
	BISON_VERSION=`$BISON -V | $SED -e 's/^bison (GNU Bison) \(.*\)$/\1/p;d'`
], [AC_MSG_ERROR([You need bison 2.4 or greater])])
AC_PROG_AWK
AC_MSG_CHECKING([if bison is >= 2.4])
AS_VERSION_COMPARE([$BISON_VERSION], [2.4], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need bison 2.4 or greater])
], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([yes])])
AC_SUBST([BISON])
AX_PROG_FLEX([
	FLEX=$LEX
	FLEX_VERSION=`$FLEX --version | $SED -e 's/^.* \(.*\)$/\1/p;d'`
], [
	AC_MSG_ERROR([You need flex 2.5.35 or greater])
])
AC_MSG_CHECKING([if flex is >= 2.5.35])
AS_VERSION_COMPARE([$FLEX_VERSION], [2.5.35], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need flex 2.5.35 or greater])
], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([yes])])
AC_SUBST([FLEX])
AC_CHECK_PROGS([_BASH], [bash])
AC_CHECK_PROGS([GPERF], [gperf])
APR_FIND_APR([], [], [1], [1])
AS_CASE([$apr_found], [yes], [
	AC_LIB_APPENDTOVAR([CPPFLAGS], [`$apr_config --includes`])
	AC_SUBST([LTLIBAPR], [`$apr_config --link-libtool`])
], [AC_MSG_ERROR([You need apr])])
VL_LIB_READLINE
AC_PROG_CXX
AC_PROG_INSTALL
AM_ICONV
AS_CASE([$am_cv_func_iconv], [yes], [], [AC_MSG_ERROR([You need iconv])])
PKG_CHECK_MODULES([WEBKIT], [webkit-1.0], [WEBKIT=yes], [
	PKG_CHECK_MODULES([WEBKIT], [WebKitGtk], [WEBKIT=yes], [WEBKIT=no])
])
AS_CASE([$WEBKIT], [yes], [
	PKG_CHECK_MODULES([LIBFFI], [libffi], [], [
		AC_CHECK_HEADERS([ffi.h ffi/ffi.h], [break])
		AS_IF([test "x$ac_cv_header_ffi_h" = xno && test "x$ac_cv_header_ffi_ffi_h" = xno], [
			AC_MSG_ERROR([You need libffi])
		])
		AC_SEARCH_LIBS([ffi_call], [ffi])
		AS_CASE([$ac_cv_search_ffi_call], [no], [
			AC_MSG_ERROR([You need libffi])
		])
	])
], [AC_MSG_WARN(No JavaScript engine!)])
CHECK_GNU_MAKE
AS_IF([test "x$_cv_gnu_make_command" = x], [AC_MSG_ERROR([You need GNU make])])
# XXX: gnustep
AC_PROG_OBJCXX
AC_CONFIG_FILES([GNUmakefile])
AC_OUTPUT
