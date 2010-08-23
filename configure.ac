AC_INIT([CGImap], [0.0.3])
AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC
AC_LANG_CPLUSPLUS

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.31)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(LIBPQXX, libpqxx >= 2.6.9)
AC_SUBST(LIBPQXX_CFLAGS)
AC_SUBST(LIBPQXX_LIBS)

#m4_include([aclocal/ax_boost_base.m4])
#m4_include([aclocal/ax_boost_regex.m4])
#m4_include([aclocal/ax_boost_program_options.m4])
#m4_include([aclocal/ax_check_zlib.m4])

AX_BOOST_BASE(1.34.1)
AX_BOOST_REGEX
AX_BOOST_PROGRAM_OPTIONS

AX_CHECK_ZLIB

AC_ARG_WITH(fcgi,
	[ --with-fcgi=<path> prefix of FCGI library.],
	[FCGI_PREFIX=$with_fcgi],
	AC_MSG_ERROR([You must call configure with the --with-fcgi option.
	This tells configure where to find the FCGI library and headers.
	e.g. --with-fcgi=/usr/local or --with-fcgi=/usr])
)

AC_SUBST(FCGI_PREFIX)
FCGI_LIBS="-L${FCGI_PREFIX}/lib -lfcgi"
FCGI_CFLAGS="-I${FCGI_PREFIX}/include"
AC_SUBST(FCGI_LIBS)
AC_SUBST(FCGI_CFLAGS)

AC_MSG_CHECKING(--enable-yajl argument)
AC_ARG_ENABLE(yajl,
    [  --enable-yajl         Enable JSON output with the YAJL library.],
    [enable_yajl=$enableval],
    [enable_yajl="no"])
AC_MSG_RESULT($enable_yajl)
AS_IF([test "x$enable_yajl" != xno],
            [AC_CHECK_LIB([yajl], [yajl_gen_alloc2],
              [AC_SUBST([YAJL_LIBS], ["-lyajl"])
               AC_DEFINE([HAVE_YAJL], [1],
                         [Define if you have YAJL JSON library])
              ],
              [AC_MSG_FAILURE(
                 [--enable-yajl was given, but test for YAJL failed])])])
AM_CONDITIONAL([HAVE_YAJL], [test "x$enable_yajl" != xno])

AC_CONFIG_FILES([
	Makefile
	src/Makefile])

AC_OUTPUT
