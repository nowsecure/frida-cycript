dnl Cycript - Optimizing JavaScript Compiler/Runtime
dnl Copyright (C) 2009-2013  Jay Freeman (saurik)

dnl GNU General Public License, Version 3 {{{
dnl
dnl Cycript is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published
dnl by the Free Software Foundation, either version 3 of the License,
dnl or (at your option) any later version.
dnl
dnl Cycript is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
dnl }}}

AC_PREREQ([2.65])
AC_INIT([Cycript], [0.9], [saurik@saurik.com], [cycript], [http://www.cycript.org/])
AC_COPYRIGHT([Copyright (C) 2009-2012  Jay Freeman (saurik)])

AC_CONFIG_SRCDIR([Console.cpp])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE([disable])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJCXX

AM_PROG_AR
AC_PROG_INSTALL

LT_INIT([disable-static])

AC_LANG([C++])

PKG_PROG_PKG_CONFIG([0.22])

AC_PATH_PROG([GIT], [git])
AC_PATH_PROG([TIME], [time])

dnl CY_CHECK_PROGS_VERSION {{{
AC_DEFUN([CY_CHECK_PROGS_VERSION], [
    AC_ARG_VAR(]$1[, ]$3[)

    AC_MSG_CHECKING(for ]$2[ >= ]$5[)
    AC_PATH_PROGS_FEATURE_CHECK(]$1[, ]$2[,
        AS_VAR_SET(version, [`$ac_path_]$1[ ]$4[`])
        AS_VAR_IF(version, [], ,
            AS_VERSION_COMPARE($version, ]$5[, ,
                ac_cv_path_]$1[=$ac_path_]$1[,
                ac_cv_path_]$1[=$ac_path_]$1[))
    , AC_MSG_RESULT(no))

    AS_IF([true], [
        AC_SUBST(]$1[, $ac_cv_path_]$1[)
        AC_MSG_RESULT($]$1[)
    ], AS_IF([true], [
        AC_MSG_ERROR(You need ]$2[ ]$5[ or greater)
    ], [
        AC_SUBST(]$1[, [:])
        AC_MSG_RESULT([no])
    ]))
]) dnl }}}

CY_CHECK_PROGS_VERSION([BISON], [bison], [GNU Project parser generator (yacc replacement)],
    [-V | $SED -e '1 s/^bison (GNU Bison) \(.*\)$/\1/p;d'], [2.3])
CY_CHECK_PROGS_VERSION([FLEX], [flex], [the fast lexical analyser generator],
    [--version | $SED -e '1 s/^@<:@^0-9.@:>@* \(@<:@0-9.@:>@*\).*$/\1/p;d'], [2.5.33])
CY_CHECK_PROGS_VERSION([GPERF], [gperf], [perfect hash function generator],
    [--version | $SED -e '1 s/^.* \(@<:@0-9.@:>@*\)$/\1/p;d'], [3.0.3])
CY_CHECK_PROGS_VERSION([GNUTAR], [tar gtar gnutar], [a very sticky lump of files],
    [--version | $SED -e '1 s/^tar (GNU tar) \(.*\)$/\1/p;d'], [1.19])

AC_MSG_CHECKING([bison for %code{}])
echo '%code{}%%_:' >conftest.y
if $BISON conftest.y -o/dev/null 2>/dev/null; then
    AC_LIB_APPENDTOVAR([CY_FILTERS], [Bison24])
    AC_MSG_RESULT([yes])
else
    AC_LIB_APPENDTOVAR([CY_FILTERS], [Bison23])
    AC_MSG_RESULT([no])
fi

rm -f conftest.y
AC_SUBST([CY_FILTERS])

dnl CY_TRY {{{
AC_DEFUN([CY_SUCCESS], [cy_good=1])
AC_DEFUN([CY_FAILURE], [cy_good=0])

AC_DEFUN([CY_TRY], [
    cy_save=$]$1[
        AC_LIB_APPENDTOVAR(]$1[, ]$2[)
        ]$3[
    ]$1[=$cy_save
    AS_IF([test "x$cy_good" = x1], ]$4[, ]$5[)
]) dnl }}}

AC_DEFUN([CY_TEST_FLAG_], [
    AC_LANG_PUSH(]$2[)
    AC_MSG_CHECKING([for ]]$4[[ ]]$1[)
    CY_TRY(]$3[, $1, [
        AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
            [CY_SUCCESS], [CY_FAILURE])
    ], [
        AC_MSG_RESULT([yes])
        AC_LIB_APPENDTOVAR(]$3[, ]$1[)
    ], [AC_MSG_RESULT([no])])
    AC_LANG_POP(]$2[)
])

AC_DEFUN([CY_TEST_FLAG], [
    CY_TEST_FLAG_(]$1[, [C], [CFLAGS], [cc])
    CY_TEST_FLAG_(]$1[, [C++], [CXXFLAGS], [c++])
    CY_TEST_FLAG_(]$1[, [Objective C++], [OBJCXXFLAGS], [c++ -ObjC])
])

AX_CFLAGS_WARN_ALL
AX_CXXFLAGS_WARN_ALL
AX_FLAGS_WARN_ALL

CY_TEST_FLAG([-Werror])

dnl XXX: this is used to support GNU Objective-C struct objc_method
CY_TEST_FLAG([-Wno-deprecated-declarations])

CY_TEST_FLAG([-Wno-dangling-else])
CY_TEST_FLAG([-Wno-overloaded-virtual])
CY_TEST_FLAG([-Wno-parentheses])
CY_TEST_FLAG([-Wno-unneeded-internal-declaration])

AC_DEFUN([CY_LT_LIB], [
    cy_save=$LIBS
    LIBS=
    ]$2[
    ]$1[=$LIBS
    LIBS=$cy_save
    AC_SUBST(]$1[)
])

dnl -arch * {{{
for cy_arch in ppc ppc64 i386 x86_64 armv6 arm64; do
    AC_MSG_CHECKING([for -arch $cy_arch])
    CY_TRY([CXXFLAGS], ["-arch $cy_arch"], [
        AC_TRY_LINK([], [], [CY_SUCCESS], [CY_FAILURE])
    ], [
        AC_MSG_RESULT([yes])
        AC_LIB_APPENDTOVAR([CY_ARCH], ["-arch $cy_arch"])
    ], [AC_MSG_RESULT([no])])
done
AC_SUBST([CY_ARCH])
dnl }}}

dnl --enable-javascript {{{
AC_DEFUN([CY_CHECK_JAVASCRIPTCORE], [
    CY_LT_LIB([LTJAVASCRIPTCORE], [AC_CHECK_FRAMEWORK([JavaScriptCore], [
        #include <JavaScriptCore/JSBase.h>
    ], [
        JSEvaluateScript(0, 0, 0, 0, 0, 0);
    ], [
        AC_SUBST([CY_EXECUTE], [1])
    ], [
        for cy_webkit_pkg in "webkit-1.0" "WebKitGtk"; do
            PKG_CHECK_MODULES([WEBKIT], [$cy_webkit_pkg], [
                AC_SUBST([CY_EXECUTE], [1])
                AC_LIB_APPENDTOVAR([CPPFLAGS], [`$PKG_CONFIG --cflags $cy_webkit_pkg`])
                AC_LIB_APPENDTOVAR([LIBS], [`$PKG_CONFIG --libs $cy_webkit_pkg`])
            ], [:]); if test "x$CY_EXECUTE" = x1; then break; fi
        done
    ])])
])

AC_DEFUN([CY_CHECK_JAVASCRIPT], [
    CY_CHECK_JAVASCRIPTCORE])

AC_ARG_ENABLE([javascript], [AS_HELP_STRING(
    [--enable-javascript@<:@=ENGINE@:>@],
    [use a JavaScript runtime (currently the only available option for ENGINE is JavaScriptCore) @<:@default=yes@:>@]
)], [
    AS_CASE([$enableval],
        [yes], [
            CY_CHECK_JAVASCRIPT],
        [no], [
            AC_SUBST([CY_EXECUTE], [0])],
        [JavaScriptCore], [
            CY_CHECK_JAVASCRIPTCORE],
        [AC_MSG_ERROR([Unknown JavaScript engine: $enableval])])
], [CY_CHECK_JAVASCRIPT])
dnl }}}

AC_ARG_ENABLE([console], AS_HELP_STRING([--disable-console], [disable console]))
AM_CONDITIONAL([CY_CONSOLE], [test "x$enable_console" != "xno"])

AM_COND_IF([CY_CONSOLE], [
dnl APR_FIND_APR {{{
AS_IF([test "x$LTLIBAPR" != x], [
    AC_SUBST([LTLIBAPR])
], [
    APR_FIND_APR([], [], [1], [1])
AS_CASE([$apr_found], [yes], [
    AC_LIB_APPENDTOVAR([CPPFLAGS], [`$apr_config --includes`])
    AC_LIB_APPENDTOVAR([LTLIBAPR], [`$apr_config --link-libtool`])
    AC_SUBST([LTLIBAPR])
], [
    AC_MSG_ERROR([missing "libapr"])
])])
dnl }}}
])

AC_DEFUN([CY_CHECK_PKG_CONFIG_LIBFFI], [
    PKG_CHECK_MODULES([LIBFFI], [libffi], [
        AC_LIB_APPENDTOVAR([CPPFLAGS], [`$PKG_CONFIG --cflags libffi`])
        AC_LIB_APPENDTOVAR([LIBS], [`$PKG_CONFIG --libs libffi`])
    ], [AC_MSG_ERROR([missing "libffi"])])])

AM_CONDITIONAL([CY_EXECUTE], [test "x$CY_EXECUTE" = x1])

AS_CASE([$CY_EXECUTE], [1], [
    AC_CHECK_HEADERS([ffi.h ffi/ffi.h], [break])

    CY_LT_LIB([LTLIBFFI], AS_IF([test "x$ac_cv_header_ffi_h" = xno && test "x$ac_cv_header_ffi_ffi_h" = xno], [
        CY_CHECK_PKG_CONFIG_LIBFFI
    ], [
        AC_SEARCH_LIBS([ffi_call], [ffi])
        AS_CASE([$ac_cv_search_ffi_call], [no], [CY_CHECK_PKG_CONFIG_LIBFFI])
    ]))

    AC_LANG_PUSH([Objective C++])

    AC_MSG_CHECKING([if we really have Objective C++])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([])], [
        CY_OBJECTIVEC=1
        AC_MSG_RESULT([yes])
    ], [AC_MSG_RESULT([no])])

    AS_IF([test "x$CY_OBJECTIVEC" = x1], [
        CY_LT_LIB([LTOBJECTIVEC], [AC_CHECK_FRAMEWORK([CoreFoundation], [
            #include <CoreFoundation/CFBase.h>
        ], [CFAllocatorGetDefault();], [
            AC_SUBST([CY_OBJECTIVEC])

            AC_CHECK_FRAMEWORK([Foundation], [
                #include <Foundation/NSObject.h>
            ], [[[[NSObject alloc] init];]])
        ], [
            AC_ARG_VAR([GNUSTEP_CONFIG], [prints information about the current gnustep installation])
            AC_CHECK_PROGS([GNUSTEP_CONFIG], [gnustep-config])

            AS_IF([test "x$GNUSTEP_CONFIG" != x], [
                AC_SUBST([CY_OBJECTIVEC])
                AC_LIB_APPENDTOVAR([OBJCXXFLAGS], [`$GNUSTEP_CONFIG --objc-flags`])
                AC_LIB_APPENDTOVAR([LIBS], [`$GNUSTEP_CONFIG --base-libs`])
            ], [AC_SUBST([CY_OBJECTIVEC], [0])])
        ])])
    ])

    AC_LANG_POP([Objective C++])
])

AM_CONDITIONAL([CY_OBJECTIVEC], [test "x$CY_OBJECTIVEC" = x1])

CY_LT_LIB([LTLIBREADLINE], [AX_LIB_READLINE])
AS_CASE([$ax_cv_lib_readline], [no], [AC_MSG_ERROR([missing "libreadline"])])

CY_LT_LIB([LTLIBTERMCAP], [AC_SEARCH_LIBS([cur_term], [termcap ncurses])])

AC_SUBST([LTLIBGCC])

AC_SUBST([LTFLAGS])
AC_SUBST([SO], [$acl_shlibext])

AS_IF([test "x$CY_EXECUTE" = x1], [
    AC_CHECK_HEADER([mach/mach.h], [
        AC_CHECK_TOOLS([_LIPO], [lipo], [AC_MSG_ERROR([missing "lipo"])])
        AC_CHECK_TOOLS([_NM], [nm], [AC_MSG_ERROR([missing "nm"])])
        AC_CHECK_TOOLS([_OTOOL], [otool], [AC_MSG_ERROR([missing "otool"])])
        AC_SUBST([CY_ATTACH_GROUP], [procmod])
        AC_SUBST([CY_MACH], [1])])])
AM_CONDITIONAL([CY_MACH], [test "x$CY_MACH" = x1])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
