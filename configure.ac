AC_INIT([CGImap], [0.0.1])
AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC
AC_LANG_CPLUSPLUS

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.32)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

AC_ARG_WITH(mysql,
	[ --with-mysql=<path> prefix of MySQL C library.],
	[MYSQL_PREFIX=$with_mysql],
	AC_MSG_ERROR([You must call configure with the --with-mysql option.
	This tells configure where to find the MySql C headers.
	e.g. --with-mysql=/usr/local or --with-mysql=/usr])
)

AC_SUBST(MYSQL_PREFIX)
MYSQL_CFLAGS="-I${MYSQL_PREFIX}/include/mysql"
AC_SUBST(MYSQL_CFLAGS)

AC_ARG_WITH(mysqlpp,
	[ --with-mysqlpp=<path> prefix of MySQL++ library.],
	[MYSQLPP_PREFIX=$with_mysqlpp],
	AC_MSG_ERROR([You must call configure with the --with-mysqlpp option.
	This tells configure where to find the MySql C++ library and headers.
	e.g. --with-mysqlpp=/usr/local or --with-mysqlpp=/usr])
)

AC_SUBST(MYSQLPP_PREFIX)
MYSQLPP_LIBS="-L${MYSQLPP_PREFIX}/lib -lmysqlpp"
MYSQLPP_CFLAGS="-I${MYSQLPP_PREFIX}/include"
AC_SUBST(MYSQLPP_LIBS)
AC_SUBST(MYSQLPP_CFLAGS)

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
