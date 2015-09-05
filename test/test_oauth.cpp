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
