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

void test_negative_changeset_ids(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (-1, 'osmosis@osmosis.com', '', '2016-04-16T15:09:00Z', 'osmosis', false);"

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (-1, -1, '2016-04-16T15:09:00Z', '2016-04-16T15:09:00Z'), "
    "  (0, -1, '2016-04-16T15:09:00Z', '2016-04-16T15:09:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
    " VALUES "
    "  (6, 90000000, 90000000,  0, true,  '2016-04-16T15:09:00Z', 3229120632, 1), "
    "  (7, 90000000, 90000000, -1, true,  '2016-04-16T15:09:00Z', 3229120632, 1); "
    );
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

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

void test_changeset(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
    "VALUES "
    "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 2);"
    );
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

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

void test_nonpublic_changeset(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
    "VALUES "
    "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 1);"
    );
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

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

void test_changeset_with_tags(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
    "VALUES "
    "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 1);"

    "INSERT INTO changeset_tags (changeset_id, k, v) "
    "VALUES "
    "  (2, 'test_key', 'test_value'), "
    "  (2, 'test_key2', 'test_value2'); "
    );
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

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

void check_changeset_with_comments_impl(
  boost::shared_ptr<data_selection> sel,
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

void test_changeset_with_comments(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
    "  (3, 'user_3@example.com', '', '2015-09-05T20:37:00Z', 'user_3', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
    "VALUES "
    "  (3, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 0); "

    "INSERT INTO changeset_comments (id, changeset_id, author_id, body, created_at, visible) "
    "VALUES "
    "  (1, 3, 3, 'a nice comment!', '2015-09-05T20:37:01Z', true), "
    "  (2, 3, 3, 'a nasty comment', '2015-09-05T20:37:10Z', false); "
    );

  try {
    boost::shared_ptr<data_selection> sel = tdb.get_data_selection();
    check_changeset_with_comments_impl(sel, false);

  } catch (const std::exception &e) {
    std::ostringstream ostr;
    ostr << e.what() << ", while include_discussion was false";
    throw std::runtime_error(ostr.str());
  }

  try {
    boost::shared_ptr<data_selection> sel = tdb.get_data_selection();
    check_changeset_with_comments_impl(sel, true);

  } catch (const std::exception &e) {
    std::ostringstream ostr;
    ostr << e.what() << ", while include_discussion was true";
    throw std::runtime_error(ostr.str());
  }
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(boost::function<void(test_database&)>(
              &test_negative_changeset_ids));

    tdb.run(boost::function<void(test_database&)>(
              &test_changeset));

    tdb.run(boost::function<void(test_database&)>(
              &test_nonpublic_changeset));

    tdb.run(boost::function<void(test_database&)>(
              &test_changeset_with_tags));

    tdb.run(boost::function<void(test_database&)>(
              &test_changeset_with_comments));

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
