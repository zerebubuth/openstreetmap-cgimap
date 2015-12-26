# ====================================================================
# Check for Crypto++
#
# Looks for an installation of the Crypto++ library:
#    https://www.cryptopp.com/
#
# If found, calls:
#    AC_SUBST(CRYPTOPP_CPPFLAGS) and
#    AC_SUBST(CRYPTOPP_LIBS) and
#    sets HAVE_CRYPTOPP

AC_DEFUN([REQUIRE_CRYPTOPP],
[
AC_ARG_WITH([cryptopp],
  [AS_HELP_STRING([--with-cryptopp=PATH],
    [full path to the Crypto++ installation.])],
  [
  if test -n "$withval" ; then
    ac_cryptopp_path="$withval"
  fi
  ],
  [ac_cryptopp_path=""])

old_cppflags=${CPPFLAGS}
old_libs=${LIBS}

CRYPTOPP_CPPFLAGS=""
CRYPTOPP_LIBS=""

if test -n "$ac_cryptopp_path" ; then
  CRYPTOPP_CPPFLAGS="-I${ac_cryptopp_path}/include"
  CRYPTOPP_LIBS="-L${ac_cryptopp_path}/lib"
fi
CRYPTOPP_LIBS="${CRYPTOPP_LIBS} -lcryptopp"

CPPFLAGS="${CPPFLAGS} ${CRYPTOPP_CPPFLAGS}"
LIBS="${LIBS} ${CRYPTOPP_LIBS}"

AC_CHECK_HEADER([cryptopp/hmac.h],
  [],
  [
  AC_MSG_ERROR([could not find Crypto++ headers, please install libcrypto++-dev])
  ])

AC_MSG_CHECKING([for Crypto++ library])
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([[
#include <cryptopp/oaep.h>
#include <cryptopp/sha.h>
]],[[
CryptoPP::OAEP<CryptoPP::SHA1, CryptoPP::P1363_MGF1>::StaticAlgorithmName();
]])],
  [AC_MSG_RESULT([yes])],
  [
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([could not find Crypto++ library, please install libcrypto++-dev])
  ])

AC_SUBST(CRYPTOPP_CPPFLAGS)
AC_SUBST(CRYPTOPP_LIBS)
AC_DEFINE(HAVE_CRYPTOPP,,[define if Crypto++ is available])

CPPFLAGS="${old_cppflags}"
LIBS="${old_libs}"
])
