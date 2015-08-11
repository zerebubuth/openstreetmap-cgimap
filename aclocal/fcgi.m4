# ============================================================
# Check for libfcgi
#
# Looks for the fcgi headers and library, which should be
# installed by libfcgi-dev or whichever platform-appropriate
# package.
#
# If it succeeds, it calls:
#   AC_SUBST(FCGI_LIBS) and AC_SUBST(FCGI_CPPFLAGS)
# and sets HAVE_FCGI.

AC_DEFUN([REQUIRE_FCGI],
[
AC_ARG_WITH(fcgi,
  [AS_HELP_STRING([--with-fcgi=<path>],
    [Path containing FCGI library (e.g: headers in <path>/include, libraries in <path>/lib).])],
  [
  if test -n "$withval" ; then
    ac_fcgi_includes="$withval/include"
    ac_fcgi_libs="$withval/lib"
  fi
  ],
  [ac_fcgi_prefix=""])
AC_ARG_WITH(fcgi-includes,
  [AS_HELP_STRING([--with-fcgi-includes=<path>],
    [Path containing FCGI header files (e.g: /usr/include).])],
  [
  if test -n "$withval" ; then
    ac_fcgi_includes="$withval"
  fi
  ],[])
AC_ARG_WITH(fcgi-libs,
  [AS_HELP_STRING([--with-fcgi-libs=<path>],
    [Path containing FCGI library files (e.g: /usr/lib).])],
  [
  if test -n "$withval" ; then
    ac_fcgi_libs="$withval"
  fi
  ],[])

dnl Check that headers for fcgi are present

ac_old_cppflags=${CPPFLAGS}
FCGI_CPPFLAGS=""
if test -n "${ac_fcgi_includes}" ; then
  FCGI_CPPFLAGS="-I${ac_fcgi_includes}"
fi
CPPFLAGS="${CPPFLAGS} ${FCGI_CPPFLAGS}"

AC_MSG_CHECKING(for fcgi headers)
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([#include <fcgiapp.h>])],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])
   AC_MSG_ERROR([cannot find fcgi headers, please install libfcgi-dev.])])

dnl Follow up by checking that the library is available

ac_old_libs=${LIBS}
FCGI_LIBS="-lfcgi"
if test -n "${ac_fcgi_libs}" ; then
  FCGI_LIBS="-L${ac_fcgi_libs} ${FCGI_LIBS}"
fi
LIBS="${LIBS} ${FCGI_LIBS}"

AC_MSG_CHECKING(for fcgi library)
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([[#include <fcgiapp.h>]],
                   [[FCGX_Init();]]
		   )],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])
   AC_MSG_ERROR([cannot find the fcgi libraries. Please install libfcgi-dev.])])

dnl Reset temporaries used in compilation

CPPFLAGS="${ac_old_cppflags}"
LIBS="${ac_old_libs}"

dnl Set variables used in the rest of the configure process

AC_SUBST(FCGI_CPPFLAGS)
AC_SUBST(FCGI_LIBS)
AC_DEFINE(HAVE_FCGI,,[define if fcgi library is available])
])
