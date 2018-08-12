#include <cassert>
#include <iostream>
#include <string>

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>
#include <set>

#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include "cgimap/basicauth.hpp"

#define ANNOTATE_EXCEPTION(stmt)                \
  {                                             \
    try {                                       \
      stmt;                                     \
    } catch (const std::exception &e) {         \
      std::ostringstream ostr;                  \
      ostr << e.what() << ", during " #stmt ;   \
        throw std::runtime_error(ostr.str());   \
    }                                           \
  }

namespace {

void assert_true(bool value) {
  if (!value) {
    throw std::runtime_error("Test failed: Expecting true, but got false.");
  }
}

template <typename T>
void assert_equal(const T &actual, const T &expected) {
  if (!(actual == expected)) {
    std::ostringstream ostr;
    ostr << "Expected `" << expected << "', but got `" << actual << "'";
    throw std::runtime_error(ostr.str());
  }
}
}

void test_password_hash() {

  PasswordHash pwd_hash;

  // test_md5_without_salt
  assert_equal<bool>(pwd_hash.check("5f4dcc3b5aa765d61d8327deb882cf99", "",
                             "password"), true);

  assert_equal<bool>(pwd_hash.check("5f4dcc3b5aa765d61d8327deb882cf99", "", "wrong"), false);

  // test_md5_with_salt
  assert_equal<bool>(pwd_hash.check("67a1e09bb1f83f5007dc119c14d663aa", "salt",
                             "password"), true);
  assert_equal<bool>(pwd_hash.check("67a1e09bb1f83f5007dc119c14d663aa", "salt",
                             "wrong"), false);
  assert_equal<bool>(pwd_hash.check("67a1e09bb1f83f5007dc119c14d663aa", "wrong",
                             "password"), false);

  // test_pbkdf2_1000_32_sha512
  assert_equal<bool>(pwd_hash.check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "password"), true);

  assert_equal<bool>(pwd_hash.check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "wrong"), false);

  assert_equal<bool>(pwd_hash.check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gwrongtoNzm/CNKe4cf7bPKwdUNrk=",
             "password"), false);

  // test_pbkdf2_10000_32_sha512
  assert_equal<bool>(pwd_hash.check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password"), true);

  assert_equal<bool>(pwd_hash.check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "wrong"), false);

  assert_equal<bool>(pwd_hash.check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtMwronguvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password"), false);

}


int main() {
  try {
    ANNOTATE_EXCEPTION(test_password_hash());


  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}



