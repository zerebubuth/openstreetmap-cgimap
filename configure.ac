AC_INIT([CGImap], [0.0.2])
AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC
AC_LANG_CPLUSPLUS

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.32)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(LIBPQXX, libpqxx >= 2.6.9)
AC_SUBST(LIBPQXX_CFLAGS)
AC_SUBST(LIBPQXX_LIBS)

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

AC_CONFIG_FILES([
	Makefile
	src/Makefile])

AC_OUTPUT
