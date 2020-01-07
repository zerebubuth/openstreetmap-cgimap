#include <iostream>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/program_options.hpp>

#include <sys/time.h>
#include <cstdio>

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
  std::shared_ptr<data_selection> sel = tdb.get_data_selection();

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
  std::shared_ptr<data_selection> sel = tdb.get_data_selection();

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(1);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  std::chrono::system_clock::time_point t = parse_time("2015-09-05T17:15:33Z");

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
  std::shared_ptr<data_selection> sel = tdb.get_data_selection();

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(4);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  std::chrono::system_clock::time_point t = parse_time("2015-09-05T20:13:23Z");

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
  std::shared_ptr<data_selection> sel = tdb.get_data_selection();

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(2);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  std::chrono::system_clock::time_point t = parse_time("2015-09-05T20:33:00Z");

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
  std::shared_ptr<data_selection> sel,
  bool include_discussion) {

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(3);
  int num = sel->select_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one changeset.");

  if (include_discussion) {
    sel->select_changeset_discussions();
  }

  std::chrono::system_clock::time_point t = parse_time("2015-09-05T20:38:00Z");

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
    std::shared_ptr<data_selection> sel = tdb.get_data_selection();
    check_changeset_with_comments_impl(sel, false);

  } catch (const std::exception &e) {
    std::ostringstream ostr;
    ostr << e.what() << ", while include_discussion was false";
    throw std::runtime_error(ostr.str());
  }

  try {
    std::shared_ptr<data_selection> sel = tdb.get_data_selection();
    check_changeset_with_comments_impl(sel, true);

  } catch (const std::exception &e) {
    std::ostringstream ostr;
    ostr << e.what() << ", while include_discussion was true";
    throw std::runtime_error(ostr.str());
  }
}

void init_changesets(test_database &tdb) {


  // Prepare users, changesets

  tdb.run_sql(R"(
	 INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
	 VALUES
	   (31, 'demo@example.com', '3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=',
                                   'sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=',
                                   '2013-11-14T02:10:00Z', 'demo', true, 'confirmed'),
	   (32, 'user_2@example.com', '', '', '2013-11-14T02:10:00Z', 'user_2', false, 'active');

	INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
	VALUES
	  (51, 31, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
	  (52, 31, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 10000),
	  (53, 31, now() at time zone 'utc' - '12 hour' ::interval,
               now() at time zone 'utc' - '11 hour' ::interval, 10000),
	  (54, 32, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
	  (55, 32, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 0);

      INSERT INTO changeset_tags(changeset_id, k, v)
         VALUES
          (52, 'created_by', 'iD 4.0.3'),
          (52, 'comment', 'Adding some perfectly squared houses ;)');

      INSERT INTO user_blocks (user_id, creator_id, reason, ends_at, needs_view)
      VALUES (31,  32, '', now() at time zone 'utc' - ('1 hour' ::interval), false);

      )"
  );

}


void test_changeset_create(test_database &tdb) {


    const std::string baseauth = "Basic ZGVtbzpwYXNzd29yZA==";
    const std::string generator = "Test";

    init_changesets(tdb);

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    // Unauthenticated user
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(
			    <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>
                          )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 401)
	  throw std::runtime_error("Expected HTTP 401 Unauthorized: wrong user/password");
    }

    // User providing wrong password
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", "Basic ZGVtbzppbnZhbGlkcGFzc3dvcmQK");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(
			    <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>
                          )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 401)
	  throw std::runtime_error("Expected HTTP 401 Unauthorized: wrong user/password");
    }

    // User is blocked (needs_view)
    {
        tdb.run_sql(R"(UPDATE user_blocks SET needs_view = true where user_id = 31;)");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(
			    <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>
                          )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 403)
	  throw std::runtime_error("Expected HTTP 403 Forbidden: user blocked (needs view)");

	tdb.run_sql(R"(UPDATE user_blocks SET needs_view = false where user_id = 31;)");
    }

    // User is blocked for 1 hour
    {
        tdb.run_sql(R"(UPDATE user_blocks
                         SET needs_view = false,
                             ends_at = now() at time zone 'utc' + ('1 hour' ::interval)
                         WHERE user_id = 31;)");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(
			    <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>
                          )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));


	if (req.response_status() != 403)
	  throw std::runtime_error("Expected HTTP 403 Forbidden: user blocked for 1 hour");

	tdb.run_sql(R"(UPDATE user_blocks
                          SET needs_view = false,
                              ends_at = now() at time zone 'utc' - ('1 hour' ::interval)
                          WHERE user_id = 31;)");
    }

    // Create new changeset
    {
      // Set changeset sequence id to new start value

        tdb.run_sql(R"(  SELECT setval('changesets_id_seq', 500, false);  )");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 200, "should have received HTTP status 200 OK");
	assert_equal<std::string>(req.body().str(), "500", "should have received changeset id 500");

	std::shared_ptr<data_selection> sel = tdb.get_data_selection();

	int num = sel->select_changesets({500});
	assert_equal<int>(num, 1, "should have selected changeset 10.");

	std::chrono::system_clock::time_point t = std::chrono::system_clock::now();

	test_formatter f;
	sel->write_changesets(f, t);
	assert_equal<size_t>(f.m_changesets.size(), 1, "should have written one changeset 500.");

	tags_t tags;
	tags.push_back(std::make_pair("comment", "Just adding some streetnames"));
	tags.push_back(std::make_pair("created_by", "JOSM 1.61"));
	assert_equal<test_formatter::changeset_t>(
	  f.m_changesets.front(),
	  test_formatter::changeset_t(
	    changeset_info(
              500, // ID
	      f.m_changesets.front().m_info.created_at, // created_at
	      f.m_changesets.front().m_info.closed_at, // closed_at
	      31, // uid
	      std::string("demo"), // display_name
	      boost::none, // bounding box
	      0, // num_changes
	      0 // comments_count
	      ),
	    tags,
	    false,
	    comments_t(),
	    t),
	  "changeset 500");

        // TODO: check users changeset count
	// TODO: check changesets_subscribers table

    }

}

void test_changeset_update(test_database &tdb) {


    const std::string baseauth = "Basic ZGVtbzpwYXNzd29yZA==";
    const std::string generator = "Test";

    init_changesets(tdb);

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    // unauthenticated user
    {

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 401, "should have received HTTP status 401 Unauthenticated");

    }

    // wrong user/password

    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51");
	req.set_header("HTTP_AUTHORIZATION", "Basic ZGVtbzppbnZhbGlkcGFzc3dvcmQK");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(
			    <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>
                          )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 401)
	  throw std::runtime_error("Expected HTTP 401 Unauthorized: wrong user/password");
    }


    // updating already closed changeset
    {

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/53");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 409, "should have received HTTP status 409 Conflict");
    }

    // updating non-existing changeset
    {

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/666");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 404, "should have received HTTP status 404 Not found");
    }

    // changeset belongs to another user
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/54");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 409, "should have received HTTP status 409 Conflict");
    }


    // Update changeset with 10k entries (may not fail)
    {

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/52");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 200, "should have received HTTP status 200 OK");

	std::shared_ptr<data_selection> sel = tdb.get_data_selection();

	int num = sel->select_changesets({52});
	assert_equal<int>(num, 1, "should have selected changeset 52.");

	std::chrono::system_clock::time_point t = std::chrono::system_clock::now();

	test_formatter f;
	sel->write_changesets(f, t);
	assert_equal<size_t>(f.m_changesets.size(), 1, "should have written one changeset 52.");

	tags_t tags;
	tags.push_back(std::make_pair("tag1", "value1"));
	tags.push_back(std::make_pair("tag2", "value2"));
	tags.push_back(std::make_pair("tag3", "value3"));
	assert_equal<test_formatter::changeset_t>(
	  f.m_changesets.front(),
	  test_formatter::changeset_t(
	    changeset_info(
	      52, // ID
	      f.m_changesets.front().m_info.created_at, // created_at
	      f.m_changesets.front().m_info.closed_at, // closed_at
	      31, // uid
	      std::string("demo"), // display_name
	      boost::none, // bounding box
	      10000, // num_changes
	      0 // comments_count
	      ),
	    tags,
	    false,
	    comments_t(),
	    t),
	  "changeset 52");

    }

}


void test_changeset_close(test_database &tdb) {


    const std::string baseauth = "Basic ZGVtbzpwYXNzd29yZA==";
    const std::string generator = "Test";

    init_changesets(tdb);

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;



    // unauthenticated user
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51/close");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 401, "should have received HTTP status 401 Unauthorized");
    }

    // Close changeset
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51/close");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 200, "should have received HTTP status 200 OK");

    }

    // changeset already closed
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/53/close");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 409, "should have received HTTP status 409 Conflict");
    }

    // updating non-existing changeset
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/666/close");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 404, "should have received HTTP status 404 Not found");
    }

    // changeset belongs to another user
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/54/close");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	assert_equal<int>(req.response_status(), 409, "should have received HTTP status 409 Conflict");
    }
}



} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(std::function<void(test_database&)>(
              &test_negative_changeset_ids));

    tdb.run(std::function<void(test_database&)>(
              &test_changeset));

    tdb.run(std::function<void(test_database&)>(
              &test_nonpublic_changeset));

    tdb.run(std::function<void(test_database&)>(
              &test_changeset_with_tags));

    tdb.run(std::function<void(test_database&)>(
              &test_changeset_with_comments));

    tdb.run(std::function<void(test_database&)>(
              &test_changeset_create));

    tdb.run(std::function<void(test_database&)>(
              &test_changeset_update));

    tdb.run(std::function<void(test_database&)>(
              &test_changeset_close));

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
