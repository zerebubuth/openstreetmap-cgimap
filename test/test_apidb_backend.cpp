#include <iostream>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>

#include <sys/time.h>
#include <stdio.h>

#include "cgimap/config.hpp"
#include "cgimap/time.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"

namespace {

template <typename T>
void assert_equal(const T& a, const T&b, const std::string &message) {
  if (a != b) {
    throw std::runtime_error(
      (boost::format(
        "Expecting %1% to be equal, but %2% != %3%")
       % message % a % b).str());
  }
}

void test_psql_array_to_vector() {
  std::string test = "{NULL}";
  std::vector<std::string> actual_values;
  std::vector<std::string> values = psql_array_to_vector(test);

  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test);
  }

  test = "{1,2}";
  values = psql_array_to_vector(test);
  actual_values.clear();
  actual_values.push_back("1");
  actual_values.push_back("2");
  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test);
  }

  test = "{\"TEST\",TEST123}";
  values = psql_array_to_vector(test);
  actual_values.clear();
  actual_values.push_back("TEST");
  actual_values.push_back("TEST123");
  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test);
  }

  test = "{\"},\\\"\",\",{}}\\\\\"}";
  values = psql_array_to_vector(test);
  actual_values.clear();
  actual_values.push_back("},\"");
  actual_values.push_back(",{}}\\");
  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test + " " +values[0] + " " +values[1]);
  }
}

void test_single_nodes(boost::shared_ptr<data_selection> sel) {
  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }
  if (sel->check_node_visibility(2) != data_selection::exists) {
    throw std::runtime_error("Node 2 should be visible, but isn't");
  }

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  ids.push_back(2);
  ids.push_back(3);
  ids.push_back(4);
  if (sel->select_nodes(ids) != 4) {
    throw std::runtime_error("Selecting 4 nodes failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting 4 nodes failed");
  }

  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(1), data_selection::exists,
    "node 1 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(2), data_selection::exists,
    "node 2 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(3), data_selection::deleted,
    "node 3 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(4), data_selection::exists,
    "node 4 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(5), data_selection::non_exist,
    "node 5 visibility");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 4, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(2, 1, 1, "2013-11-14T02:10:01Z", 1, std::string("user_1"), true),
      0.1, 0.1,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[2], "third node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(4, 1, 4, "2015-03-02T19:25:00Z", boost::none, boost::none, true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[3], "fourth (anonymous) node written");
}

void test_dup_nodes(boost::shared_ptr<data_selection> sel) {
  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  ids.push_back(1);
  ids.push_back(1);
  if (sel->select_nodes(ids) != 1) {
    throw std::runtime_error("Selecting 3 duplicates of 1 node failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting the same node failed");
  }

  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(1), data_selection::exists,
    "node 1 visibility");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 1, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
}

void test_nonce_store(boost::shared_ptr<oauth::store> store) {
  // can use a nonce
  assert_equal<bool>(true, store->use_nonce("abcdef", 0), "first use of nonce");

  // can't use it twice
  assert_equal<bool>(false, !store->use_nonce("abcdef", 0),
                     "second use of the same nonce");

  // can use the same nonce with a different timestamp
  assert_equal<bool>(true, store->use_nonce("abcdef", 1),
                     "use of nonce with a different timestamp");

  // or the same timestamp with a different nonce
  assert_equal<bool>(true, store->use_nonce("abcdeg", 0),
                     "use of nonce with a different nonce string");
}

void test_allow_read_api(boost::shared_ptr<oauth::store> store) {
  assert_equal<bool>(
    true, store->allow_read_api("OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA"),
    "valid token allows reading API");

  assert_equal<bool>(
    false, store->allow_read_api("wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC"),
    "non-authorized token does not allow reading API");

  assert_equal<bool>(
    false, store->allow_read_api("Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf"),
    "invalid token does not allow reading API");
}

void test_get_user_id_for_token(boost::shared_ptr<oauth::store> store) {
  assert_equal<boost::optional<osm_user_id_t> >(
    1, store->get_user_id_for_token("OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA"),
    "valid token belongs to user 1");

  assert_equal<boost::optional<osm_user_id_t> >(
    1, store->get_user_id_for_token("wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC"),
    "non-authorized token belongs to user 1");

  assert_equal<boost::optional<osm_user_id_t> >(
    1, store->get_user_id_for_token("Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf"),
    "invalid token belongs to user 1");

  assert_equal<boost::optional<osm_user_id_t> >(
    boost::none,
    store->get_user_id_for_token("____5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf"),
    "non-existent token does not belong to anyone");
}

void test_negative_changeset_ids(boost::shared_ptr<data_selection> sel) {
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(6), data_selection::exists,
    "node 6 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(7), data_selection::exists,
    "node 7 visibility");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(6);
  ids.push_back(7);
  if (sel->select_nodes(ids) != 2) {
    throw std::runtime_error("Selecting 2 nodes failed");
  }

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 2, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(6, 1, 0, "2016-04-16T15:09:00Z", boost::none, boost::none, true),
      9.0, 9.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(7, 1, -1, "2016-04-16T15:09:00Z", boost::none, boost::none, true),
      9.0, 9.0,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
}

void test_changeset(boost::shared_ptr<data_selection> sel) {
  assert_equal<bool>(sel->supports_changesets(), true,
                     "apidb should support changesets.");

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(1);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  boost::posix_time::ptime t = parse_time("2015-09-05T17:15:33Z");

  test_formatter f;
  sel->write_changesets(f, t);
  assert_equal<size_t>(f.m_changesets.size(), 1,
                       "should have written one changeset.");

  assert_equal<test_formatter::changeset_t>(
    f.m_changesets.front(),
    test_formatter::changeset_t(
      changeset_info(
        1, // ID
        "2013-11-14T02:10:00Z", // created_at
        "2013-11-14T03:10:00Z", // closed_at
        1, // uid
        std::string("user_1"), // display_name
        boost::none, // bounding box
        2, // num_changes
        0 // comments_count
        ),
      tags_t(),
      false,
      comments_t(),
      t),
    "changesets");
}

void test_nonpublic_changeset(boost::shared_ptr<data_selection> sel) {
  assert_equal<bool>(sel->supports_changesets(), true,
                     "apidb should support changesets.");

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(4);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  boost::posix_time::ptime t = parse_time("2015-09-05T20:13:23Z");

  test_formatter f;
  sel->write_changesets(f, t);
  assert_equal<size_t>(f.m_changesets.size(), 1,
                       "should have written one changeset.");

  assert_equal<test_formatter::changeset_t>(
    f.m_changesets.front(),
    test_formatter::changeset_t(
      changeset_info(
        4, // ID
        "2013-11-14T02:10:00Z", // created_at
        "2013-11-14T03:10:00Z", // closed_at
        boost::none, // uid
        boost::none, // display_name
        boost::none, // bounding box
        1, // num_changes
        0 // comments_count
        ),
      tags_t(),
      false,
      comments_t(),
      t),
    "changesets");
}

void test_changeset_with_tags(boost::shared_ptr<data_selection> sel) {
  assert_equal<bool>(sel->supports_changesets(), true,
                     "apidb should support changesets.");

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(2);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  boost::posix_time::ptime t = parse_time("2015-09-05T20:33:00Z");

  test_formatter f;
  sel->write_changesets(f, t);
  assert_equal<size_t>(f.m_changesets.size(), 1,
                       "should have written one changeset.");

  tags_t tags;
  tags.push_back(std::make_pair("test_key", "test_value"));
  tags.push_back(std::make_pair("test_key2", "test_value2"));
  assert_equal<test_formatter::changeset_t>(
    f.m_changesets.front(),
    test_formatter::changeset_t(
      changeset_info(
        2, // ID
        "2013-11-14T02:10:00Z", // created_at
        "2013-11-14T03:10:00Z", // closed_at
        1, // uid
        std::string("user_1"), // display_name
        boost::none, // bounding box
        1, // num_changes
        0 // comments_count
        ),
      tags,
      false,
      comments_t(),
      t),
    "changesets should be equal.");
}

void check_changeset_with_comments(boost::shared_ptr<data_selection> sel,
                                   bool include_discussion) {
  assert_equal<bool>(sel->supports_changesets(), true,
                     "apidb should support changesets.");

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(3);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  if (include_discussion) {
    sel->select_changeset_discussions();
  }

  boost::posix_time::ptime t = parse_time("2015-09-05T20:38:00Z");

  test_formatter f;
  sel->write_changesets(f, t);
  assert_equal<size_t>(f.m_changesets.size(), 1,
                       "should have written one changeset.");

  comments_t comments;
  {
    changeset_comment_info comment;
    comment.author_id = 3;
    comment.body = "a nice comment!";
    comment.created_at = "2015-09-05T20:37:01Z";
    comment.author_display_name = "user_3";
    comments.push_back(comment);
  }
  // note that we don't see the non-visible one in the database.
  assert_equal<test_formatter::changeset_t>(
    f.m_changesets.front(),
    test_formatter::changeset_t(
      changeset_info(
        3, // ID
        "2013-11-14T02:10:00Z", // created_at
        "2013-11-14T03:10:00Z", // closed_at
        1, // uid
        std::string("user_1"), // display_name
        boost::none, // bounding box
        0, // num_changes
        1 // comments_count
        ),
      tags_t(),
      include_discussion,
      comments,
      t),
    "changesets should be equal.");
}

void test_changeset_with_comments_not_including_discussions(boost::shared_ptr<data_selection> sel) {
  try {
    check_changeset_with_comments(sel, false);

  } catch (const std::exception &e) {
    std::ostringstream ostr;
    ostr << e.what() << ", while include_discussion was false";
    throw std::runtime_error(ostr.str());
  }
}

void test_changeset_with_comments_including_discussions(boost::shared_ptr<data_selection> sel) {
  try {
    check_changeset_with_comments(sel, true);

  } catch (const std::exception &e) {
    std::ostringstream ostr;
    ostr << e.what() << ", while include_discussion was true";
    throw std::runtime_error(ostr.str());
  }
}

class empty_data_selection
  : public data_selection {
public:

  virtual ~empty_data_selection() {}

  void write_nodes(output_formatter &formatter) {}
  void write_ways(output_formatter &formatter) {}
  void write_relations(output_formatter &formatter) {}
  void write_changesets(output_formatter &formatter,
                        const boost::posix_time::ptime &now) {}

  visibility_t check_node_visibility(osm_nwr_id_t id) {}
  visibility_t check_way_visibility(osm_nwr_id_t id) {}
  visibility_t check_relation_visibility(osm_nwr_id_t id) {}

  int select_nodes(const std::vector<osm_nwr_id_t> &) { return 0; }
  int select_ways(const std::vector<osm_nwr_id_t> &) { return 0; }
  int select_relations(const std::vector<osm_nwr_id_t> &) { return 0; }
  int select_nodes_from_bbox(const bbox &bounds, int max_nodes) { return 0; }
  void select_nodes_from_relations() {}
  void select_ways_from_nodes() {}
  void select_ways_from_relations() {}
  void select_relations_from_ways() {}
  void select_nodes_from_way_nodes() {}
  void select_relations_from_nodes() {}
  void select_relations_from_relations() {}
  void select_relations_members_of_relations() {}
  bool supports_changesets() { return false; }
  int select_changesets(const std::vector<osm_changeset_id_t> &) { return 0; }
  void select_changeset_discussions() {}

  struct factory
    : public data_selection::factory {
    virtual ~factory() {}
    virtual boost::shared_ptr<data_selection> make_selection() {
      return boost::make_shared<empty_data_selection>();
    }
  };
};

struct recording_rate_limiter
  : public rate_limiter {
  ~recording_rate_limiter() {}

  bool check(const std::string &key) {
    m_keys_seen.insert(key);
    return true;
  }

  void update(const std::string &key, int bytes) {
    m_keys_seen.insert(key);
  }

  bool saw_key(const std::string &key) {
    return m_keys_seen.count(key) > 0;
  }

private:
  std::set<std::string> m_keys_seen;
};


void test_oauth_end_to_end(boost::shared_ptr<oauth::store> store) {
  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;
  boost::shared_ptr<data_selection::factory> factory =
    boost::make_shared<empty_data_selection::factory>();

  test_request req;
  req.set_header("SCRIPT_URL", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_URI",
                 "http://www.openstreetmap.org/api/0.6/relation/165475/full");
  req.set_header("HTTP_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_ACCEPT_ENCODING",
                 "gzip;q=1.0,deflate;q=0.6,identity;q=0.3");
  req.set_header("HTTP_ACCEPT", "*/*");
  req.set_header("HTTP_USER_AGENT", "OAuth gem v0.4.7");
  req.set_header("HTTP_AUTHORIZATION",
                 "OAuth oauth_consumer_key=\"x3tHSMbotPe5fBlItMbg\", "
                 "oauth_nonce=\"dvu3eTk8i1uvj8zQ8Wef91UF6ngQdlTA3xQ2vEf7xU\", "
                 "oauth_signature=\"ewKFprItE5uaDHKFu3IVzuEHbno%3D\", "
                 "oauth_signature_method=\"HMAC-SHA1\", "
                 "oauth_timestamp=\"1475844649\", "
                 "oauth_token=\"15zpwgGjdjBu1DD65X7kcHzaWqfQpvqmMtqa3ZIO\", "
                 "oauth_version=\"1.0\"");
  req.set_header("HTTP_X_REQUEST_ID", "V-eaKX8AAQEAAF4UzHwAAAHt");
  req.set_header("HTTP_X_FORWARDED_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_X_FORWARDED_SERVER", "www.openstreetmap.org");
  req.set_header("HTTP_CONNECTION", "Keep-Alive");
  req.set_header("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
  req.set_header("SERVER_SIGNATURE", "<address>Apache/2.4.18 (Ubuntu) Server "
                 "at www.openstreetmap.org Port 80</address>");
  req.set_header("SERVER_SOFTWARE", "Apache/2.4.18 (Ubuntu)");
  req.set_header("SERVER_NAME", "www.openstreetmap.org");
  req.set_header("SERVER_ADDR", "127.0.0.1");
  req.set_header("SERVER_PORT", "80");
  req.set_header("REMOTE_ADDR", "127.0.0.1");
  req.set_header("DOCUMENT_ROOT", "/srv/www.openstreetmap.org/rails/public");
  req.set_header("REQUEST_SCHEME", "http");
  req.set_header("SERVER_PROTOCOL", "HTTP/1.1");
  req.set_header("REQUEST_METHOD", "GET");
  req.set_header("QUERY_STRING", "");
  req.set_header("REQUEST_URI", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_NAME", "/api/0.6/relation/165475/full");

  assert_equal<boost::optional<std::string> >(
    std::string("ewKFprItE5uaDHKFu3IVzuEHbno="),
    oauth::detail::hashed_signature(req, *store),
    "hashed signatures");

  process_request(req, limiter, generator, route, factory, store);

  assert_equal<int>(404, req.response_status(), "response status");
  assert_equal<bool>(false, limiter.saw_key("addr:127.0.0.1"),
                     "saw addr:127.0.0.1 as a rate limit key");
  assert_equal<bool>(true, limiter.saw_key("user:1"),
                     "saw user:1 as a rate limit key");
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    test_psql_array_to_vector();

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_single_nodes));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_dup_nodes));

    tdb.run(boost::function<void(boost::shared_ptr<oauth::store>)>(
        &test_nonce_store));

    tdb.run(boost::function<void(boost::shared_ptr<oauth::store>)>(
        &test_allow_read_api));

    tdb.run(boost::function<void(boost::shared_ptr<oauth::store>)>(
        &test_get_user_id_for_token));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_negative_changeset_ids));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
              &test_changeset));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
              &test_nonpublic_changeset));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
              &test_changeset_with_tags));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
              &test_changeset_with_comments_not_including_discussions));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
              &test_changeset_with_comments_including_discussions));

    tdb.run(boost::function<void(boost::shared_ptr<oauth::store>)>(
              &test_oauth_end_to_end));

  } catch (const test_database::setup_error &e) {
    std::cout << "Unable to set up test database: " << e.what() << std::endl;
    return 77;

  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cout << "Unknown exception type." << std::endl;
    return 99;
  }

  return 0;
}
