#include "cgimap/oauth.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>

#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

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
  if (actual != expected) {
    std::ostringstream ostr;
    ostr << "Expected `" << expected << "', but got `" << actual << "'";
    throw std::runtime_error(ostr.str());
  }
}

struct test_request : public request {
  test_request(const std::string &method,
               const std::string &scheme,
               const std::string &authority,
               const std::string &port,
               const std::string &path,
               const std::string &get_params,
               const boost::optional<std::string> &auth_header);
  virtual ~test_request();

  const char *get_param(const char *key);
  void dispose();

protected:
  void write_header_info(int status, const headers_t &headers);

  boost::shared_ptr<output_buffer> get_buffer_internal();
  void finish_internal();

private:
  std::string method, scheme, authority, port, path, get_params;
  boost::optional<std::string> auth_header;
};

test_request::test_request(const std::string &method_,
                           const std::string &scheme_,
                           const std::string &authority_,
                           const std::string &port_,
                           const std::string &path_,
                           const std::string &get_params_,
                           const boost::optional<std::string> &auth_header_)
  : method(method_), scheme(scheme_), authority(authority_), port(port_),
    path(path_), get_params(get_params_), auth_header(auth_header_) {
}

test_request::~test_request() {
}

const char *test_request::get_param(const char *key) {
  if (std::strncmp(key, "HTTP_AUTHORIZATION", 19) == 0) {
    return bool(auth_header) ? (auth_header->c_str()) : NULL;
  } else if (std::strncmp(key, "PATH_INFO", 10) == 0) {
    return path.c_str();
  } else if (std::strncmp(key, "QUERY_STRING", 13) == 0) {
    return get_params.c_str();
  } else if (std::strncmp(key, "REQUEST_METHOD", 15) == 0) {
    return method.c_str();
  } else if (std::strncmp(key, "REQUEST_URI", 12) == 0) {
    return path.c_str();
  } else if (std::strncmp(key, "SERVER_NAME", 12) == 0) {
    return authority.c_str();
  } else if (std::strncmp(key, "SERVER_PORT", 12) == 0) {
    return port.c_str();
  } else if (std::strncmp(key, "HTTPS", 6) == 0) {
    return (scheme == "https") ? scheme.c_str() : NULL;
  } else {
    return NULL;
  }
}

void test_request::dispose() {
}

void test_request::write_header_info(int status, const headers_t &headers) {
  throw std::runtime_error("test_request::write_header_info unimplemented.");
}

boost::shared_ptr<output_buffer> test_request::get_buffer_internal() {
  throw std::runtime_error("test_request::get_buffer_internal unimplemented.");
}

void test_request::finish_internal() {
  throw std::runtime_error("test_request::finish_internal unimplemented.");
}

} // anonymous namespace

void oauth_check_signature_base_string() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    auth_header);

  assert_equal<std::string>(
    oauth::detail::normalise_request_url(req),
    "http://photos.example.net/photos");

  assert_equal<boost::optional<std::string> >(
    oauth::detail::normalise_request_parameters(req),
    std::string("file=vacation.jpg&oauth_consumer_key=dpf43f3p2l4k3l03&oauth_nonce=kllo9940pd9333jh&oauth_signature_method=HMAC-SHA1&oauth_timestamp=1191242096&oauth_token=nnch734d00sl2jdk&oauth_version=1.0&size=original"));

  assert_equal<boost::optional<std::string> >(
    oauth::detail::signature_base_string(req),
    std::string("GET&http%3A%2F%2Fphotos.example.net%2Fphotos&file%3Dvacation.jpg%26oauth_consumer_key%3Ddpf43f3p2l4k3l03%26oauth_nonce%3Dkllo9940pd9333jh%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1191242096%26oauth_token%3Dnnch734d00sl2jdk%26oauth_version%3D1.0%26size%3Doriginal"));
}

void oauth_check_signature_base_string2() {
  // generated using http://nouncer.com/oauth/signature.html
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://PHOTOS.example.net:8001/Photos\", oauth_consumer_key=\"dpf43f3%2B%2Bp%2B%232l4k3l03\", oauth_token=\"nnch734d%280%290sl2jdk\", oauth_nonce=\"kllo~9940~pd9333jh\", oauth_timestamp=\"1191242096\", oauth_signature_method=\"HMAC-SHA1\", oauth_version=\"1.0\", oauth_signature=\"tTFyqivhutHiglPvmyilZlHm5Uk%3D\"");
  test_request req(
    "GET",
    "http", "PHOTOS.example.net", "8001", "Photos", "photo%20size=300%25&title=Back%20of%20%24100%20Dollars%20Bill",
    auth_header);

  assert_equal<std::string>(
    oauth::detail::normalise_request_url(req),
    "http://photos.example.net:8001/Photos");

  assert_equal<boost::optional<std::string> >(
    oauth::detail::normalise_request_parameters(req),
    std::string("oauth_consumer_key=dpf43f3%2B%2Bp%2B%232l4k3l03&oauth_nonce=kllo~9940~pd9333jh&oauth_signature_method=HMAC-SHA1&oauth_timestamp=1191242096&oauth_token=nnch734d%280%290sl2jdk&oauth_version=1.0&photo%20size=300%25&title=Back%20of%20%24100%20Dollars%20Bill"));

  assert_equal<boost::optional<std::string> >(
    oauth::detail::signature_base_string(req),
    std::string("GET&http%3A%2F%2Fphotos.example.net%3A8001%2FPhotos&oauth_consumer_key%3Ddpf43f3%252B%252Bp%252B%25232l4k3l03%26oauth_nonce%3Dkllo~9940~pd9333jh%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1191242096%26oauth_token%3Dnnch734d%25280%25290sl2jdk%26oauth_version%3D1.0%26photo%2520size%3D300%2525%26title%3DBack%2520of%2520%2524100%2520Dollars%2520Bill"));
}

void oauth_check_signature_base_string3() {
  // generated using http://nouncer.com/oauth/signature.html
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"https://www.example.com/path\", oauth_consumer_key=\"abcdef\", oauth_token=\"bcdefg\", oauth_nonce=\"123456\", oauth_timestamp=\"1443648660\", oauth_signature_method=\"HMAC-SHA1\", oauth_version=\"1.0\", oauth_signature=\"TWS6VYOQSpNZt6%2FTNp%2Bgbgbnfaw%3D\"");
  test_request req(
    "POST",
    "https", "www.example.com", "443", "path", "",
    auth_header);

  assert_equal<std::string>(
    oauth::detail::normalise_request_url(req),
    "https://www.example.com/path");

  assert_equal<boost::optional<std::string> >(
    oauth::detail::normalise_request_parameters(req),
    std::string("oauth_consumer_key=abcdef&oauth_nonce=123456&oauth_signature_method=HMAC-SHA1&oauth_timestamp=1443648660&oauth_token=bcdefg&oauth_version=1.0"));

  assert_equal<boost::optional<std::string> >(
    oauth::detail::signature_base_string(req),
    std::string("POST&https%3A%2F%2Fwww.example.com%2Fpath&oauth_consumer_key%3Dabcdef%26oauth_nonce%3D123456%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1443648660%26oauth_token%3Dbcdefg%26oauth_version%3D1.0"));
}

void oauth_check_signature_base_string4() {
  // generated using http://nouncer.com/oauth/signature.html
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://example.com/request\", oauth_consumer_key=\"9djdj82h48djs9d2\", oauth_token=\"kkk9d7dh3k39sjv7\", oauth_nonce=\"7d8f3e4a\", oauth_timestamp=\"137131201\", oauth_signature_method=\"HMAC-SHA1\", oauth_version=\"1.0\", oauth_signature=\"InXuTE4pXaeiQxfEYTM4Cs8Fuds%3D\"");
  test_request req(
    "POST",
    "http", "example.com", "80", "request", "b5=%3D%253D&a3=a&c%40=&a2=r%20b&c2&a3=2+q",
    auth_header);

  assert_equal<std::string>(
    oauth::detail::normalise_request_url(req),
    "http://example.com/request");

  assert_equal<boost::optional<std::string> >(
    oauth::detail::normalise_request_parameters(req),
    std::string("a2=r%20b&a3=2%20q&a3=a&b5=%3D%253D&c%40=&c2=&oauth_consumer_key=9djdj82h48djs9d2&oauth_nonce=7d8f3e4a&oauth_signature_method=HMAC-SHA1&oauth_timestamp=137131201&oauth_token=kkk9d7dh3k39sjv7&oauth_version=1.0"));

  assert_equal<boost::optional<std::string> >(
    oauth::detail::signature_base_string(req),
    std::string("POST&http%3A%2F%2Fexample.com%2Frequest&a2%3Dr%2520b%26a3%3D2%2520q%26a3%3Da%26b5%3D%253D%25253D%26c%2540%3D%26c2%3D%26oauth_consumer_key%3D9djdj82h48djs9d2%26oauth_nonce%3D7d8f3e4a%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D137131201%26oauth_token%3Dkkk9d7dh3k39sjv7%26oauth_version%3D1.0"));
}

void oauth_check_valid_signature_header() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    auth_header);

  assert_true(oauth::is_valid_signature(req));
}

void oauth_check_invalid_signature_header() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photo", "file=vacation.jpg&size=original",
    auth_header);

  assert_true(!oauth::is_valid_signature(req));
}

void oauth_check_valid_signature_params() {
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original&oauth_consumer_key=dpf43f3p2l4k3l03&oauth_token=nnch734d00sl2jdk&oauth_signature_method=HMAC-SHA1&oauth_signature=tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D&oauth_timestamp=1191242096&oauth_nonce=kllo9940pd9333jh&oauth_version=1.0",
    boost::none);

  assert_true(oauth::is_valid_signature(req));
}

int main() {
  try {
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string());
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string2());
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string3());
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string4());
    //ANNOTATE_EXCEPTION(oauth_check_valid_signature_header());
    ANNOTATE_EXCEPTION(oauth_check_invalid_signature_header());
    //ANNOTATE_EXCEPTION(oauth_check_valid_signature_params());

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}
