AC_INIT([CGImap], 
        [0.2.0], 
	[https://github.com/zerebubuth/openstreetmap-cgimap/issues], 
	[cgimap-0.2.0], 
	[https://github.com/zerebubuth/openstreetmap-cgimap])
AM_INIT_AUTOMAKE([subdir-objects parallel-tests])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([include/config.h])

AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC
AC_LANG_CPLUSPLUS

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.31)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(LIBPQXX, libpqxx >= 3.0)
AC_SUBST(LIBPQXX_CFLAGS)
AC_SUBST(LIBPQXX_LIBS)

#m4_include([aclocal/ax_boost_base.m4])
#m4_include([aclocal/ax_boost_regex.m4])
#m4_include([aclocal/ax_boost_system.m4])
#m4_include([aclocal/ax_boost_program_options.m4])
#m4_include([aclocal/ax_boost_date_time.m4])
#m4_include([aclocal/ax_check_zlib.m4])

PKG_CHECK_MODULES(LIBMEMCACHED, libmemcached >= 0.31)
AC_SUBST(LIBMEMCACHED_CFLAGS)
AC_SUBST(LIBMEMCACHED_LIBS)

AX_BOOST_BASE([1.43], , [AC_MSG_ERROR([Boost libraries are required for building CGImap. Please install libboost-dev.])])
AX_BOOST_REGEX
AX_BOOST_SYSTEM
AX_BOOST_PROGRAM_OPTIONS
AX_BOOST_DATE_TIME

AX_CHECK_ZLIB

AC_ARG_WITH(fcgi,
	[AS_HELP_STRING([--with-fcgi=<path>], [Path containing FCGI library (e.g: /usr).])],
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

AC_MSG_CHECKING(whether to enable JSON output using YAJL)
AC_ARG_ENABLE(yajl,
    [AS_HELP_STRING([--enable-yajl], [Enable JSON output with the YAJL library. API 0.7 only.])],
    [enable_yajl=$enableval],
    [enable_yajl="no"])
AC_MSG_RESULT($enable_yajl)
AS_IF([test "x$enable_yajl" != xno],
            [AC_CHECK_LIB([yajl], [yajl_gen_alloc2],
              [AC_SUBST([YAJL_LIBS], ["-lyajl"])
               AC_DEFINE([HAVE_YAJL], [1],
                         [Define if you have YAJL JSON library])
              ],
	      [AC_CHECK_LIB([yajl], [yajl_gen_config],
	      	[AC_SUBST([YAJL_LIBS], ["-lyajl"])
	      	 AC_DEFINE([HAVE_YAJL], [1],
                 	   [Define if you have YAJL JSON library])
		 AC_DEFINE([HAVE_YAJL2], [1],
			   [Define if you have version 2 of the YAJL library])
		],
	        [AC_MSG_FAILURE(
                  [--enable-yajl was given, but test for YAJL failed])])])])
AM_CONDITIONAL([HAVE_YAJL], [test "x$enable_yajl" != xno])

AC_MSG_CHECKING([whether to enable experimental API 0.7 features])
AC_ARG_ENABLE([api07],
	[AS_HELP_STRING([--enable-api07],
		[Enable experimental API 0.7 features])],
	[enable_api07="yes"],
	[enable_api07="no"])
AC_MSG_RESULT($enable_api07)
AS_IF([test "x$enable_api07" == xyes],
	[AC_DEFINE([ENABLE_API07], [1], [Define for experimental API 0.7 features.])])
AM_CONDITIONAL([ENABLE_API07], [test "x$enable_api07" == xyes])

AC_MSG_CHECKING([whether to enable APIDB backend])
AC_ARG_ENABLE([apidb],
	[AS_HELP_STRING([--enable-apidb],
		[Enable APIDB backend, as used by the OSM servers (default: enabled)])],
	[case "${enableval}" in
	  yes | no ) enable_apidb="${enableval}" ;;
	  * ) AC_MSG_ERROR(value for --enable-apidb should be yes or no) ;;
	 esac],
	[enable_apidb="yes"])
AC_MSG_RESULT($enable_apidb)
AS_IF([test "x$enable_apidb" == xyes],
      [AC_DEFINE([ENABLE_APIDB], [1], [Define for APIDB backend support.])])
AM_CONDITIONAL([ENABLE_APIDB], [test "x$enable_apidb" == xyes])

AC_MSG_CHECKING([whether to enable PGSNAPSHOT backend])
AC_ARG_ENABLE([pgsnapshot],
	[AS_HELP_STRING([--enable-pgsnapshot],
		[Enable pgsnapshot backend (default: enabled)])],
	[case "${enableval}" in
	  yes | no ) enable_pgsnapshot="${enableval}" ;;
	  * ) AC_MSG_ERROR(value for --enable-pgsnapshot should be yes or no) ;;
	 esac],
	[enable_pgsnapshot="yes"])
AC_MSG_RESULT($enable_pgsnapshot)
AS_IF([test "x$enable_pgsnapshot" == xyes],
      [AC_DEFINE([ENABLE_PGSNAPSHOT], [1], [Define for PGSNAPSHOT backend support.])])
AM_CONDITIONAL([ENABLE_PGSNAPSHOT], [test "x$enable_pgsnapshot" == xyes])
AC_CONFIG_FILES([
	Makefile
	src/Makefile])

AC_OUTPUT
