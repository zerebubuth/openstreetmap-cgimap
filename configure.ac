AC_INIT([CGImap],
        [0.6.0],
        [https://github.com/zerebubuth/openstreetmap-cgimap/issues],
        [cgimap-0.6.0],
        [https://github.com/zerebubuth/openstreetmap-cgimap])
AM_INIT_AUTOMAKE([subdir-objects parallel-tests])
LT_INIT
AC_SUBST([LIBTOOL_DEPS])

AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([include/cgimap/config.hpp])
AC_CONFIG_MACRO_DIR([aclocal])

AC_PROG_CXX
# C++11 support is apparently required now due to recent memcached
# headers including <cinttypes> rather than <tr1/cinttypes> without
# itself checking for C++11 support.
AX_CXX_COMPILE_STDCXX_11([],[mandatory])
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC
AC_LANG_CPLUSPLUS

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.31)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(LIBPQXX, libpqxx >= 4.0)
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
AX_BOOST_FILESYSTEM
AX_BOOST_LOCALE

# the boost libraries are required, not optional, but the
# AX_BOOST_* checks only error if the headers aren't
# available?
AS_IF([test "x$BOOST_REGEX_LIB" == "x"],
      [AC_MSG_ERROR([Boost::Regex library not detected, please install libboost-regex-dev.])],)
AS_IF([test "x$BOOST_SYSTEM_LIB" == "x"],
      [AC_MSG_ERROR([Boost::System library not detected, please install libboost-system-dev.])],)
AS_IF([test "x$BOOST_PROGRAM_OPTIONS_LIB" == "x"],
      [AC_MSG_ERROR([Boost::ProgramOptions library not detected, please install libboost-program-options-dev.])],)
AS_IF([test "x$BOOST_DATE_TIME_LIB" == "x"],
      [AC_MSG_ERROR([Boost::DateTime library not detected, please install libboost-date-time-dev.])],)
## travis-ci doesn't have a boost with locale yet (grumble,
## grumble), so put up with not having this lib available.
#
#AS_IF([test "x$BOOST_LOCALE_LIB" == "x"],
#		[AC_MSG_ERROR([Boost::Locale library not detected, please install libboost-locale-dev.])],)

AX_CHECK_ZLIB
REQUIRE_FCGI
REQUIRE_CRYPTOPP

AC_MSG_CHECKING([whether to enable experimental other API 0.6 calls])
AC_ARG_ENABLE([experimental],
	[AS_HELP_STRING([--enable-experimental],
		[Enable experimental other API 0.6 calls])],
	[enable_experimental="yes"],
	[enable_experimental="no"])
AC_MSG_RESULT($enable_experimental)
AS_IF([test "x$enable_experimental" == xyes],
	    [AC_DEFINE([ENABLE_EXPERIMENTAL], [1], [Define for experimental other API 0.6 calls.])])
AM_CONDITIONAL([ENABLE_EXPERIMENTAL], [test "x$enable_experimental" == xyes])

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

AC_ARG_WITH([clang-format],
	[AS_HELP_STRING([--with-clang-format@<:@=ARG@:>@],
	   [Enable re-formatting source code with clang-format. @<:@ARG=no@:>@ ])],
	[case "${withval}" in
	  yes ) prog_clang_format="clang-format-3.6" ;;
	  no ) prog_clang_format="no" ;;
	  * ) prog_clang_format="${withval}" ;;
	 esac],
	[prog_clang_format="no"])
AS_IF([test "x$prog_clang_format" != "xno"],
		[AC_CHECK_PROGS([CLANG_FORMAT], "$prog_clang_format", [no])])
AM_CONDITIONAL([HAVE_CLANG_FORMAT], [test "x$CLANG_FORMAT" != "xno"])

AC_OUTPUT
