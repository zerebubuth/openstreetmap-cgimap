/* -*- coding: utf-8 -*- */
#include "cgimap/http.hpp"
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace {

void assert_eq(const std::string &actual, const std::string &expected) {
  if (actual != expected) {
    std::ostringstream ostr;
    ostr << "Test failed: Expecting \"" << expected << "\", but got \"" << actual << "\".";
    throw std::runtime_error(ostr.str());
  }
}

template <typename T>
void assert_equal(const T &actual, const T &expected) {
  if (actual != expected) {
    std::ostringstream ostr;
    ostr << "Expected `" << expected << "', but got `" << actual << "'";
    throw std::runtime_error(ostr.str());
  }
}

void http_check_urlencoding() {
  // RFC 3986 section 2.5
  assert_eq(http::urlencode("ア"), "%E3%82%A2");
  assert_eq(http::urlencode("À"), "%C3%80");

  // RFC 3986 - unreserved characters not encoded
  assert_eq(http::urlencode("abcdefghijklmnopqrstuvwxyz"), "abcdefghijklmnopqrstuvwxyz");
  assert_eq(http::urlencode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  assert_eq(http::urlencode("0123456789"), "0123456789");
  assert_eq(http::urlencode("-._~"), "-._~");

  // RFC 3986 - % must be encoded
  assert_eq(http::urlencode("%"), "%25");
}

void http_check_urldecoding() {
  assert_eq(http::urldecode("%E3%82%A2"), "ア");
  assert_eq(http::urldecode("%C3%80"), "À");

  // RFC 3986 - uppercase A-F are equivalent to lowercase a-f
  assert_eq(http::urldecode("%e3%82%a2"), "ア");
  assert_eq(http::urldecode("%c3%80"), "À");
}

void http_check_parse_params() {
  typedef std::vector<std::pair<std::string, std::string> > params_t;
  params_t params = http::parse_params("a2=r%20b&a3=2%20q&a3=a&b5=%3D%253D&c%40=&c2=");

  assert_equal<std::size_t>(params.size(), 6);
  assert_eq(params[0].first, "a2");   assert_eq(params[0].second, "r%20b");
  assert_eq(params[1].first, "a3");   assert_eq(params[1].second, "2%20q");
  assert_eq(params[2].first, "a3");   assert_eq(params[2].second, "a");
  assert_eq(params[3].first, "b5");   assert_eq(params[3].second, "%3D%253D");
  assert_eq(params[4].first, "c%40"); assert_eq(params[4].second, "");
  assert_eq(params[5].first, "c2");   assert_eq(params[5].second, "");
}

} // anonymous namespace

int main() {
  try {
    http_check_urlencoding();
    http_check_urldecoding();
    http_check_parse_params();

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}
