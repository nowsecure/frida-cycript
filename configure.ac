#AC_PREREQ([2.65])
AC_INIT([Cycript], [0.9], [saurik@saurik.com], [cycript], [http://www.cycript.org/])
AC_CONFIG_SRCDIR([Console.cpp])
AX_PROG_BISON([
	BISON=`echo $YACC | $SED -e 's/ -y//'`
	BISON_VERSION=`$BISON -V | $SED -e 's/^bison (GNU Bison) \(.*\)$/\1/p;d'`
], [AC_MSG_ERROR([You need bison 2.4 or greater])])
AC_PROG_AWK
AC_MSG_CHECKING([if bison is >= 2.4])
AX_COMPARE_VERSION([$BISON_VERSION], [ge], [2.4], [AC_MSG_RESULT([yes])], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need bison 2.4 or greater])
])
AC_SUBST([BISON])
AC_PROG_LEX
AC_MSG_CHECKING([for flex])
AS_IF([test "`echo \"$LEX\" | $SED 's/^.*\(flex\).*$/\1/'`" = "flex"], [
	AC_MSG_RESULT([yes])
	AC_SUBST([FLEX], [$LEX])
], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need flex])
])
APR_FIND_APR([], [], [1], [1])
AS_CASE([$apr_found], [yes], [
	AC_LIB_APPENDTOVAR([CPPFLAGS], [`$apr_config --includes`])
	AC_SUBST([APR], [`$apr_config --link-ld`])
], [AC_MSG_ERROR([You need apr])])
AC_PROG_CXX
AC_PROG_INSTALL
AM_ICONV
AS_CASE([$am_cv_func_iconv], [yes], [], [AC_MSG_ERROR([You need iconv])])
PKG_CHECK_MODULES([WEBKIT], [webkit-1.0], [WEBKIT=yes], [
	PKG_CHECK_MODULES([WEBKIT], [WebKitGtk], [WEBKIT=yes], [WEBKIT=no])
])
AS_CASE([$WEBKIT], [yes], [
	PKG_CHECK_MODULES([LIBFFI], [libffi])
], [AC_MSG_WARN(No JavaScript engine!)])
CHECK_GNU_MAKE
AS_IF([test x$_cv_gnu_make_command = x], [AC_MSG_ERROR([You need GNU make])])
# XXX: gnustep
#AC_PROG_OBJCXX
