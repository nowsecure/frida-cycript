AC_PREREQ([2.65])
AC_INIT([Cycript], [0.9], [saurik@saurik.com], [cycript], [http://www.cycript.org/])
AC_CONFIG_SRCDIR([Console.cpp])
AC_CONFIG_AUX_DIR([.])
LT_INIT([disable-static])
PKG_PROG_PKG_CONFIG([0.22])
CHECK_GNU_MAKE
AC_SUBST([GMAKE], [$_cv_gnu_make_command])
AS_IF([test "x$GMAKE" = x], [AC_MSG_ERROR([You need GNU make])])
AC_PROG_CXX
AC_PROG_OBJCXX
AC_ARG_WITH([frameworks], [AS_HELP_STRING(
	[--with-frameworks=DIRS],
	[add to DIRS (space separated) to the Darwin include path for frameworks with -F]
)], [
	for framework in $withval; do
		AC_LIB_APPENDTOVAR([CFLAGS], [-F$framework])
		AC_LIB_APPENDTOVAR([CXXFLAGS], [-F$framework])
		AC_LIB_APPENDTOVAR([OBJCCXXFLAGS], [-F$framework])
	done
])
AC_LANG([C++])
AC_ARG_VAR([BISON], [GNU Project parser generator (yacc replacement)])
AS_IF([test "x$BISON" = x], [AX_PROG_BISON([
	AC_SUBST([BISON], [`echo $YACC | $SED -e 's/ -y//'`])
], [AC_MSG_ERROR([You need bison 2.3 or greater])])])
AC_MSG_CHECKING([if bison is >= 2.3])
BISON_VERSION=[`$BISON -V | $SED -e 's/^bison (GNU Bison) \(.*\)$/\1/p;d'`]
AS_VERSION_COMPARE([$BISON_VERSION], [2.3], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need bison 2.3 or greater])
], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([yes])])
AC_ARG_VAR([FLEX], [the fast lexical analyser generator])
AS_IF([test "x$FLEX" = x], [AX_PROG_FLEX([
	AC_SUBST([FLEX], [$LEX])
], [AC_MSG_ERROR([You need flex 2.5.33 or greater])])])
FLEX_VERSION=[`$FLEX --version | $SED -e 's/^.* \([0-9.]*\)$/\1/p;d'`]
AC_MSG_CHECKING([if flex is >= 2.5.33])
AS_VERSION_COMPARE([$FLEX_VERSION], [2.5.33], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need flex 2.5.33 or greater])
], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([yes])])
AC_CHECK_PROGS([BASH], [bash])
AC_ARG_VAR([GPERF], [perfect hash function generator])
AC_CHECK_PROGS([GPERF], [gperf])
AC_MSG_CHECKING([if gperf >= 3.0.3])
GPERF_VERSION=[`$GPERF --version | $SED -e 's/^.* \([0-9.]*\)$/\1/p;d'`]
AS_VERSION_COMPARE([$GPERF_VERSION], [3.0.3], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([You need gperf 3.0.3 or greater])
], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([yes])])
AC_CHECK_PROGS([TIME], [time])
APR_FIND_APR([], [], [1], [1])
AS_CASE([$apr_found], [yes], [
	AC_LIB_APPENDTOVAR([CPPFLAGS], [`$apr_config --includes`])
	AC_SUBST([LTLIBAPR], [`$apr_config --link-libtool`])
], [AC_MSG_ERROR([You need apr])])
VL_LIB_READLINE
AC_PROG_INSTALL
AC_LANG_PUSH([C])
AM_ICONV
AC_LANG_POP([C])
AS_CASE([$am_cv_func_iconv], [yes], [], [AC_MSG_ERROR([You need iconv])])
AC_DEFUN([CY_CHECK_JAVASCRIPTCORE], [
	PKG_CHECK_MODULES([WEBKIT], [webkit-1.0], [
		CY_EXECUTE=1
		AC_LIB_APPENDTOVAR([CXXFLAGS], [`$PKG_CONFIG --cflags webkit-1.0`])
		AC_LIB_APPENDTOVAR([LIBS], [`$PKG_CONFIG --libs webkit-1.0`])
	], [
		PKG_CHECK_MODULES([WEBKIT], [WebKitGtk], [
			CY_EXECUTE=1
			AC_LIB_APPENDTOVAR([CXXFLAGS], [`$PKG_CONFIG --cflags WebKitGtk`])
			AC_LIB_APPENDTOVAR([LIBS], [`$PKG_CONFIG --libs WebKitGtk`])
		], [
			AC_CHECK_FRAMEWORK([JavaScriptCore], [
				#include <JavaScriptCore/JSBase.h>
			], [JSEvaluateScript(0, 0, 0, 0, 0, 0);], [CY_EXECUTE=1])
		])
	])
])
AC_DEFUN([CY_CHECK_JAVASCRIPT], [
	CY_CHECK_JAVASCRIPTCORE
])
AC_ARG_ENABLE([javascript], [AS_HELP_STRING(
	[--enable-javascript@<:@=ENGINE@:>@],
	[use a JavaScript runtime (currently the only available option for ENGINE is JavaScriptCore) @<:@default=yes@:>@]
)], [
	AS_CASE([$enableval], [yes], [CY_CHECK_JAVASCRIPT], [no], [
		AC_SUBST([CY_EXECUTE], [0])
	], [JavaScriptCore], [
		CY_CHECK_JAVASCRIPTCORE
	], [AC_MSG_ERROR([Unknown JavaScript engine: $enableval])])
], [CY_CHECK_JAVASCRIPT])
AS_CASE([$CY_EXECUTE], [1], [
	AC_SUBST([CY_EXECUTE])
	PKG_CHECK_MODULES([LIBFFI], [libffi], [
		AC_LIB_APPENDTOVAR([CXXFLAGS], [`$PKG_CONFIG --cflags libffi`])
		AC_LIB_APPENDTOVAR([LIBS], [`$PKG_CONFIG --libs libffi`])
	], [
		AC_CHECK_HEADERS([ffi.h ffi/ffi.h], [break])
		AS_IF([test "x$ac_cv_header_ffi_h" = xno && test "x$ac_cv_header_ffi_ffi_h" = xno], [
			AC_MSG_ERROR([You need libffi])
		])
		AC_SEARCH_LIBS([ffi_call], [ffi])
		AS_CASE([$ac_cv_search_ffi_call], [no], [
			AC_MSG_ERROR([You need libffi])
		])
	])
	AC_LANG_PUSH([Objective C++])
	AC_MSG_CHECKING([if we really have Objective C++])
	AC_COMPILE_IFELSE([
		// This space intentionally left blank
	], [
		CY_OBJECTIVEC=1
		AC_MSG_RESULT([yes])
	], [AC_MSG_RESULT([no])])
	AS_IF([test "x$CY_OBJECTIVEC" = x1], [
		AC_CHECK_FRAMEWORK([CoreFoundation], [
			#include <CoreFoundation/CFBase.h>
		], [CFAllocatorGetDefault();], [
			AC_SUBST([CY_OBJECTIVEC])
			AC_CHECK_FRAMEWORK([Foundation], [
				#include <Foundation/NSObject.h>
			], [[[[NSObject alloc] init];]])
			AC_CHECK_FRAMEWORK([WebKit], [
				#include <WebKit/WebScriptObject.h>
			], [[[[WebScriptObject alloc] init];]])
		], [
			AC_ARG_VAR([GNUSTEP_CONFIG], [prints information about the current gnustep installation])
			AC_CHECK_PROGS([GNUSTEP_CONFIG], [gnustep-config])
			AS_IF([test "x$GNUSTEP_CONFIG" != x], [
				AC_SUBST([CY_OBJECTIVEC])
				AC_LIB_APPENDTOVAR([OBJCXXFLAGS], [`$GNUSTEP_CONFIG --objc-flags`])
				AC_LIB_APPENDTOVAR([LIBS], [`$GNUSTEP_CONFIG --base-libs`])
			], [AC_SUBST([CY_OBJECTIVEC], [0])])
		])
	])
	AC_LANG_POP([Objective C++])
])
AS_IF([test "x$CY_OBJECTIVEC" = x1], [
	AC_CHECK_HEADER([mach/mach.h], [
		AS_CASE([$OTOOL], [:], [AC_MSG_WARN([Missing otool?])], [AC_SUBST([CY_OBJECTIVEC_MACH], [1])])
	])
])
AS_IF([test "x$GMAKE" != xmake], [
	AC_CONFIG_FILES([Makefile])
])
AC_CONFIG_FILES([GNUmakefile])
AC_OUTPUT
