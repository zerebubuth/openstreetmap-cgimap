#include "cgimap/oauth.hpp"
#include "cgimap/oauth_io.hpp"

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

struct test_request : public request {
  test_request(const std::string &method,
               const std::string &scheme,
               const std::string &authority,
               const std::string &port,
               const std::string &path,
               const std::string &get_params,
               const boost::optional<time_t> &timestamp,
               const boost::optional<std::string> &auth_header);
  virtual ~test_request();

  const char *get_param(const char *key);
  void dispose();

  boost::posix_time::ptime get_current_time() const;

protected:
  void write_header_info(int status, const headers_t &headers);

  boost::shared_ptr<output_buffer> get_buffer_internal();
  void finish_internal();

private:
  std::string method, scheme, authority, port, path, get_params;
  boost::posix_time::ptime timestamp;
  boost::optional<std::string> auth_header;
};

test_request::test_request(const std::string &method_,
                           const std::string &scheme_,
                           const std::string &authority_,
                           const std::string &port_,
                           const std::string &path_,
                           const std::string &get_params_,
                           const boost::optional<time_t> &timestamp_,
                           const boost::optional<std::string> &auth_header_)
  : method(method_), scheme(scheme_), authority(authority_), port(port_),
    path(path_), get_params(get_params_), auth_header(auth_header_) {
  if (bool(timestamp_)) {
    timestamp = boost::posix_time::from_time_t(*timestamp_);
  }
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

boost::posix_time::ptime test_request::get_current_time() const {
  return timestamp;
}

} // anonymous namespace

void oauth_check_signature_base_string() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    1191242096,
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
    1191242096,
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
    1443648660,
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
    137131201,
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

struct test_secret_store
  : public oauth::secret_store
  , public oauth::nonce_store
  , public oauth::token_store {
  test_secret_store(const std::string &consumer_key,
                    const std::string &consumer_secret,
                    const std::string &token_id,
                    const std::string &token_secret)
    : m_consumer_key(consumer_key)
    , m_consumer_secret(consumer_secret)
    , m_token_id(token_id)
    , m_token_secret(token_secret) {
  }

  boost::optional<std::string> consumer_secret(const std::string &key) {
    if (key == m_consumer_key) {
      return m_consumer_secret;
    }
    return boost::none;
  }

  boost::optional<std::string> token_secret(const std::string &id) {
    if (id == m_token_id) {
      return m_token_secret;
    }
    return boost::none;
  }

  bool use_nonce(const std::string &nonce,
                 uint64_t timestamp) {
    boost::tuple<std::string, uint64_t> tuple =
      boost::make_tuple(nonce, timestamp);
    if (m_nonces.count(tuple) > 0) {
      return false;
    }
    m_nonces.insert(tuple);
    return true;
  }

  bool allow_read_api(const std::string &id) {
    return id == m_token_id;
  }

  boost::optional<osm_user_id_t> get_user_id_for_token(const std::string &id) {
    return boost::none;
  }

  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t) {
    return std::set<osm_user_role_t>();
  }

private:
  std::string m_consumer_key, m_consumer_secret, m_token_id, m_token_secret;
  std::set<boost::tuple<std::string, uint64_t> > m_nonces;
};

void oauth_check_base64() {
  // examples from https://en.wikipedia.org/wiki/Base64#Examples
  assert_equal<std::string>(
    oauth::detail::base64_encode("any carnal pleasure."),
    "YW55IGNhcm5hbCBwbGVhc3VyZS4=");

  assert_equal<std::string>(
    oauth::detail::base64_encode("any carnal pleasure"),
    "YW55IGNhcm5hbCBwbGVhc3VyZQ==");

  assert_equal<std::string>(
    oauth::detail::base64_encode("any carnal pleasur"),
    "YW55IGNhcm5hbCBwbGVhc3Vy");

  assert_equal<std::string>(
    oauth::detail::base64_encode("any carnal pleasu"),
    "YW55IGNhcm5hbCBwbGVhc3U=");

  assert_equal<std::string>(
    oauth::detail::base64_encode("any carnal pleas"),
    "YW55IGNhcm5hbCBwbGVhcw==");

  assert_equal<std::string>(
    oauth::detail::base64_encode(""),
    "");
}

void oauth_check_hmac_sha1() {
  const unsigned char expected[] = {
    0xf0, 0x17, 0x31, 0xab, 0xa4, 0x4c, 0xa5, 0x6d, 0x27, 0x99, 0xa6, 0x90, 0xe5,
    0xda, 0x6b, 0x64, 0x75, 0xc3, 0x44, 0x0f };

  std::string hash = oauth::detail::hmac_sha1(
    "abcdef123456",
    "Testing.");

  assert_equal<std::string>(
    hash,
    std::string(reinterpret_cast<const char *>(expected), sizeof(expected) / sizeof(unsigned char)));
}

void oauth_check_signature_hmac_sha1_1() {
  // generated using http://nouncer.com/oauth/signature.html
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://PHOTOS.example.net:8001/Photos\", oauth_consumer_key=\"dpf43f3%2B%2Bp%2B%232l4k3l03\", oauth_token=\"nnch734d%280%290sl2jdk\", oauth_nonce=\"kllo~9940~pd9333jh\", oauth_timestamp=\"1191242096\", oauth_signature_method=\"HMAC-SHA1\", oauth_version=\"1.0\", oauth_signature=\"MH9NDodF4I%2FV6GjYYVChGaKCtnk%3D\"");
  test_request req(
    "GET",
    "http", "PHOTOS.example.net", "8001", "Photos", "type=%C3%97%C2%90%C3%97%E2%80%A2%C3%97%CB%9C%C3%97%E2%80%A2%C3%97%E2%80%98%C3%97%E2%80%A2%C3%97%C2%A1&scenario=%C3%97%C2%AA%C3%97%C2%90%C3%97%E2%80%A2%C3%97%C2%A0%C3%97%E2%80%9D",
    1191242096,
    auth_header);

  assert_equal<std::string>(
    oauth::detail::normalise_request_url(req),
    "http://photos.example.net:8001/Photos");

  assert_equal<boost::optional<std::string> >(
    oauth::detail::normalise_request_parameters(req),
    std::string("oauth_consumer_key=dpf43f3%2B%2Bp%2B%232l4k3l03&oauth_nonce=kllo~9940~pd9333jh&oauth_signature_method=HMAC-SHA1&oauth_timestamp=1191242096&oauth_token=nnch734d%280%290sl2jdk&oauth_version=1.0&scenario=%C3%97%C2%AA%C3%97%C2%90%C3%97%E2%80%A2%C3%97%C2%A0%C3%97%E2%80%9D&type=%C3%97%C2%90%C3%97%E2%80%A2%C3%97%CB%9C%C3%97%E2%80%A2%C3%97%E2%80%98%C3%97%E2%80%A2%C3%97%C2%A1"));

  assert_equal<boost::optional<std::string> >(
    oauth::detail::signature_base_string(req),
    std::string("GET&http%3A%2F%2Fphotos.example.net%3A8001%2FPhotos&oauth_consumer_key%3Ddpf43f3%252B%252Bp%252B%25232l4k3l03%26oauth_nonce%3Dkllo~9940~pd9333jh%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1191242096%26oauth_token%3Dnnch734d%25280%25290sl2jdk%26oauth_version%3D1.0%26scenario%3D%25C3%2597%25C2%25AA%25C3%2597%25C2%2590%25C3%2597%25E2%2580%25A2%25C3%2597%25C2%25A0%25C3%2597%25E2%2580%259D%26type%3D%25C3%2597%25C2%2590%25C3%2597%25E2%2580%25A2%25C3%2597%25CB%259C%25C3%2597%25E2%2580%25A2%25C3%2597%25E2%2580%2598%25C3%2597%25E2%2580%25A2%25C3%2597%25C2%25A1"));

  test_secret_store store("dpf43f3++p+#2l4k3l03", "kd9@4h%%4f93k423kf44",
                          "nnch734d(0)0sl2jdk",   "pfkkd#hi9_sl-3r=4s00");
  assert_equal<boost::optional<std::string> >(
    oauth::detail::hashed_signature(req, store),
    std::string("MH9NDodF4I/V6GjYYVChGaKCtnk="));
}

void oauth_check_signature_plaintext_1() {
  // generated using http://nouncer.com/oauth/signature.html
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://PHOTOS.example.net:8001/Photos\", oauth_consumer_key=\"dpf43f3%2B%2Bp%2B%23%26l4k3l03\", oauth_token=\"nnch73%26d%280%290sl2jdk\", oauth_nonce=\"kllo~9940~pd9333jh\", oauth_timestamp=\"1191242096\", oauth_signature_method=\"PLAINTEXT\", oauth_version=\"1.0\", oauth_signature=\"kd9%25404h%2525%2525%2526f93k423kf44%26pfkkd%2523hi9_s%2526-3r%253D4s00\"");
  test_request req(
    "GET",
    "http", "PHOTOS.example.net", "8001", "Photos", "photo%20size=300%25&title=Back%20of%20%24100%20Dollars%20Bill",
    1191242096,
    auth_header);

  assert_equal<std::string>(
    oauth::detail::normalise_request_url(req),
    "http://photos.example.net:8001/Photos");

  test_secret_store store("dpf43f3++p+#&l4k3l03", "kd9@4h%%&f93k423kf44",
                          "nnch73&d(0)0sl2jdk",   "pfkkd#hi9_s&-3r=4s00");
  assert_equal<boost::optional<std::string> >(
    oauth::detail::hashed_signature(req, store),
    std::string("kd9%404h%25%25%26f93k423kf44&pfkkd%23hi9_s%26-3r%3D4s00"));
}

void oauth_check_valid_signature_header() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    1191242096,
    auth_header);

  test_secret_store store("dpf43f3p2l4k3l03", "kd94hf93k423kf44",
                          "nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
  oauth::validity::validity expected(
    oauth::validity::copacetic("nnch734d00sl2jdk"));
  assert_equal(expected,
               oauth::is_valid_signature(req, store, store, store));
}

void oauth_check_invalid_signature_header() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photo", "file=vacation.jpg&size=original",
    1191242096,
    auth_header);

  test_secret_store store("dpf43f3p2l4k3l03", "kd94hf93k423kf44",
                          "nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
  assert_equal(oauth::validity::validity(oauth::validity::unauthorized("")),
               oauth::is_valid_signature(req, store, store, store));
}

void oauth_check_valid_signature_params() {
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original&oauth_consumer_key=dpf43f3p2l4k3l03&oauth_token=nnch734d00sl2jdk&oauth_signature_method=HMAC-SHA1&oauth_signature=tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D&oauth_timestamp=1191242096&oauth_nonce=kllo9940pd9333jh&oauth_version=1.0",
    boost::none,
    boost::none);

  test_secret_store store("dpf43f3p2l4k3l03", "kd94hf93k423kf44",
                          "nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
  assert_equal(oauth::validity::validity(
                 oauth::validity::copacetic("nnch734d00sl2jdk")),
               oauth::is_valid_signature(req, store, store, store));
}

void oauth_check_missing_signature() {
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    boost::none,
    boost::none);

  test_secret_store store("dpf43f3p2l4k3l03", "kd94hf93k423kf44",
                          "nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
  assert_equal(oauth::validity::validity(oauth::validity::not_signed()),
               oauth::is_valid_signature(req, store, store, store));
}

void oauth_check_valid_signature_header_2() {
  boost::optional<std::string> auth_header = std::string("OAuth oauth_consumer_key=\"x3tHSMbotPe5fBlItMbg\", oauth_nonce=\"ZGsGj6qzGYUhSLHJWUC8tyW6RbxOQuX4mv6PKj0mU\", oauth_signature=\"H%2Fxl6jdk4dC0WaONfohWfZhcHYA%3D\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\"1475754589\", oauth_token=\"15zpwgGjdjBu1DD65X7kcHzaWqfQpvqmMtqa3ZIO\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "www.openstreetmap.org", "80", "/api/0.6/relation/165475/full", "",
    1475754589,
    auth_header);

  assert_equal<boost::optional<std::string> >(
    oauth::detail::signature_base_string(req),
    std::string("GET&http%3A%2F%2Fwww.openstreetmap.org%2Fapi%2F0.6%2Frelation%2F165475%2Ffull&oauth_consumer_key%3Dx3tHSMbotPe5fBlItMbg%26oauth_nonce%3DZGsGj6qzGYUhSLHJWUC8tyW6RbxOQuX4mv6PKj0mU%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1475754589%26oauth_token%3D15zpwgGjdjBu1DD65X7kcHzaWqfQpvqmMtqa3ZIO%26oauth_version%3D1.0"));

  std::string consumer_key("x3tHSMbotPe5fBlItMbg");
  std::string consumer_secret("1NZRJ0u2o7OilPDe60nfZsKJTC7RUZPrNfYwGBjATw");
  std::string token_id("15zpwgGjdjBu1DD65X7kcHzaWqfQpvqmMtqa3ZIO");
  std::string token_secret("H3Vb9Kgf4LpTyVlft5xsI9MwzknQsTu6CkHE0qK3");

  test_secret_store store(consumer_key, consumer_secret, token_id, token_secret);

  assert_equal<boost::optional<std::string> >(
    oauth::detail::hashed_signature(req, store),
    std::string("H/xl6jdk4dC0WaONfohWfZhcHYA="));

  oauth::validity::copacetic copacetic(token_id);
  oauth::validity::validity expected(copacetic);
  assert_equal(oauth::is_valid_signature(req, store, store, store), expected);
}

void oauth_check_almost_expired_signature() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    1191242096 + 86370,
    auth_header);

  test_secret_store store("dpf43f3p2l4k3l03", "kd94hf93k423kf44",
                          "nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
  oauth::validity::validity expected(
    oauth::validity::copacetic("nnch734d00sl2jdk"));
  assert_equal(expected,
               oauth::is_valid_signature(req, store, store, store));
}

void oauth_check_expired_signature() {
  boost::optional<std::string> auth_header = std::string("OAuth realm=\"http://photos.example.net/\", oauth_consumer_key=\"dpf43f3p2l4k3l03\", oauth_token=\"nnch734d00sl2jdk\", oauth_signature_method=\"HMAC-SHA1\", oauth_signature=\"tR3%2BTy81lMeYAr%2FFid0kMTYa%2FWM%3D\", oauth_timestamp=\"1191242096\", oauth_nonce=\"kllo9940pd9333jh\", oauth_version=\"1.0\"");
  test_request req(
    "GET",
    "http", "photos.example.net", "80", "photos", "file=vacation.jpg&size=original",
    1191242096 + 86430,
    auth_header);

  test_secret_store store("dpf43f3p2l4k3l03", "kd94hf93k423kf44",
                          "nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
  oauth::validity::validity expected(
    oauth::validity::unauthorized("Timestamp is too far in the past."));
  assert_equal(expected,
               oauth::is_valid_signature(req, store, store, store));
}

int main() {
  try {
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string());
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string2());
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string3());
    ANNOTATE_EXCEPTION(oauth_check_signature_base_string4());
    ANNOTATE_EXCEPTION(oauth_check_base64());
    ANNOTATE_EXCEPTION(oauth_check_hmac_sha1());
    ANNOTATE_EXCEPTION(oauth_check_signature_hmac_sha1_1());
    ANNOTATE_EXCEPTION(oauth_check_signature_plaintext_1());
    ANNOTATE_EXCEPTION(oauth_check_valid_signature_header());
    ANNOTATE_EXCEPTION(oauth_check_invalid_signature_header());
    ANNOTATE_EXCEPTION(oauth_check_valid_signature_params());
    ANNOTATE_EXCEPTION(oauth_check_missing_signature());
    ANNOTATE_EXCEPTION(oauth_check_valid_signature_header_2());
    ANNOTATE_EXCEPTION(oauth_check_almost_expired_signature());
    ANNOTATE_EXCEPTION(oauth_check_expired_signature());

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}
