AC_INIT([CGImap],
        [0.8.8],
        [https://github.com/zerebubuth/openstreetmap-cgimap/issues],
        [cgimap-0.8.8],
        [https://github.com/zerebubuth/openstreetmap-cgimap])
AM_INIT_AUTOMAKE([subdir-objects parallel-tests])
LT_INIT
AC_SUBST([LIBTOOL_DEPS])

AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([include/cgimap/config.hpp])
AC_CONFIG_MACRO_DIR([aclocal])

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([17],[],[mandatory])
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC
AC_LANG([C++])

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= 2.6.31)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(LIBPQXX, libpqxx >= 6.0)
AC_SUBST(LIBPQXX_CFLAGS)
AC_SUBST(LIBPQXX_LIBS)

PKG_CHECK_MODULES(FMT, fmt >= 6.0)
AC_SUBST(FMT_CFLAGS)
AC_SUBST(FMT_LIBS)

PKG_CHECK_MODULES([ARGON2], [libargon2])
AC_SUBST(argon2_lib, [", libargon2"])

PKG_CHECK_MODULES(LIBMEMCACHED, libmemcached >= 0.31)
AC_SUBST(LIBMEMCACHED_CFLAGS)
AC_SUBST(LIBMEMCACHED_LIBS)

AX_BOOST_BASE([1.43], , [AC_MSG_ERROR([Boost libraries are required for building CGImap. Please install libboost-dev.])])
AX_BOOST_PROGRAM_OPTIONS

# the boost libraries are required, not optional, but the
# AX_BOOST_* checks only error if the headers aren't
# available?
AS_IF([test "x$BOOST_PROGRAM_OPTIONS_LIB" == "x"],
      [AC_MSG_ERROR([Boost::ProgramOptions library not detected, please install libboost-program-options-dev.])],)


AX_CHECK_ZLIB
REQUIRE_FCGI
REQUIRE_CRYPTOPP

AC_MSG_CHECKING(whether to enable JSON output using YAJL)
AC_ARG_ENABLE(yajl,
    [AS_HELP_STRING([--enable-yajl], [Enable JSON output with the YAJL library.])],
    [enable_yajl=$enableval],
    [enable_yajl="yes"])
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



#AC_MSG_CHECKING([whether to enable experimental API 0.7 features])
#AC_ARG_ENABLE([api07],
#	[AS_HELP_STRING([--enable-api07],
#		[Enable experimental API 0.7 features])],
#	[enable_api07="yes"],
#	[enable_api07="no"])
#AC_MSG_RESULT($enable_api07)
#AS_IF([test "x$enable_api07" == xyes],
#	[AC_DEFINE([ENABLE_API07], [1], [Define for experimental API 0.7 features.])])
#AM_CONDITIONAL([ENABLE_API07], [test "x$enable_api07" == xyes])

AC_MSG_CHECKING([whether to enable FMT header only])
AC_ARG_ENABLE([fmt_header],
	[AS_HELP_STRING([--enable-fmt-header],
		[Enable FMT header only mode (default: enabled)])],
	[case "${enableval}" in
	  yes | no ) enable_fmt_header="${enableval}" ;;
	  * ) AC_MSG_ERROR(value for ---enable-fmt-header should be yes or no) ;;
	 esac],
	[enable_fmt_header="yes"])
AC_MSG_RESULT($enable_fmt_header)
AS_IF([test "x$enable_fmt_header" == xyes],
      [AC_DEFINE([FMT_HEADER_ONLY], [1], [Define for FMT header only.])])
AM_CONDITIONAL([ENABLE_FMT_HEADER], [test "x$enable_fmt_header" == xyes])

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
