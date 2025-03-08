/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <fmt/core.h>

#include <sys/time.h>
#include <cstdio>

#include "cgimap/time.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"
#include "cgimap/request_context.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"


#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>


class DatabaseTestsFixture
{
public:
  static void setTestDatabaseSchema(const std::filesystem::path& db_sql) {
    test_db_sql = db_sql;
  }

protected:
  DatabaseTestsFixture() = default;
  inline static std::filesystem::path test_db_sql{"test/structure.sql"};
  static test_database tdb;
};

test_database DatabaseTestsFixture::tdb{};

struct CGImapListener : Catch::TestEventListenerBase, DatabaseTestsFixture {

    using TestEventListenerBase::TestEventListenerBase; // inherit constructor

    void testRunStarting( Catch::TestRunInfo const& testRunInfo ) override {
      // load database schema when starting up tests
      tdb.setup(test_db_sql);
    }

    void testCaseStarting( Catch::TestCaseInfo const& testInfo ) override {
      tdb.testcase_starting();
    }

    void testCaseEnded( Catch::TestCaseStats const& testCaseStats ) override {
      tdb.testcase_ended();
    }
};

CATCH_REGISTER_LISTENER( CGImapListener )



TEST_CASE_METHOD( DatabaseTestsFixture, "test_negative_changeset_ids", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

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


  }

  SECTION("Validate data") {

    REQUIRE(sel->check_node_visibility(6) == data_selection::exists);

    REQUIRE(sel->check_node_visibility(7) == data_selection::exists);

    REQUIRE(sel->select_nodes({6,7}) == 2);

    test_formatter f;
    sel->write_nodes(f);
    REQUIRE(f.m_nodes.size() ==  2);

    REQUIRE(
        test_formatter::node_t(
        element_info(6, 1, 0, "2016-04-16T15:09:00Z", {}, {}, true),
        9.0, 9.0,
        tags_t()
        ) == f.m_nodes[0]);

    REQUIRE(
      test_formatter::node_t(
        element_info(7, 1, -1, "2016-04-16T15:09:00Z", {}, {}, true),
        9.0, 9.0,
        tags_t()
        ) == f.m_nodes[1]);
  }
}


TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {
    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

      "INSERT INTO changesets (id, user_id, min_lat, max_lat, min_lon, max_lon, created_at, closed_at, num_changes) "
      "VALUES "
      "  (1, 1, 387436644, 535639226, -91658156, 190970588, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 2);"
      );
  }

  SECTION("Validate data") {

    int num = sel->select_changesets({1});
    REQUIRE(num ==  1);

    std::chrono::system_clock::time_point t = parse_time("2015-09-05T17:15:33Z");

    test_formatter f;
    sel->write_changesets(f, t);
    REQUIRE(f.m_changesets.size() == 1);

    REQUIRE(
      f.m_changesets.front() ==
      test_formatter::changeset_t(
        changeset_info(
          1, // ID
          "2013-11-14T02:10:00Z", // created_at
          "2013-11-14T03:10:00Z", // closed_at
          1, // uid
          std::string("user_1"), // display_name
          bbox{38.7436644, -9.1658156, 53.5639226, 19.0970588}, // bounding box
          2, // num_changes
          0 // comments_count
          ),
        tags_t(),
        false,
        comments_t(),
        t));
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_nonpublic_changeset", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
      "VALUES "
      "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 1);"
      );
  }

  SECTION("Validate data") {

    int num = sel->select_changesets({4});
    REQUIRE(num == 1);

    std::chrono::system_clock::time_point t = parse_time("2015-09-05T20:13:23Z");

    test_formatter f;
    sel->write_changesets(f, t);
    REQUIRE(f.m_changesets.size() == 1);

    REQUIRE(
      f.m_changesets.front() ==
      test_formatter::changeset_t(
        changeset_info(
          4, // ID
          "2013-11-14T02:10:00Z", // created_at
          "2013-11-14T03:10:00Z", // closed_at
          {}, // uid
          {}, // display_name
          {}, // bounding box
          1, // num_changes
          0 // comments_count
          ),
        tags_t(),
        false,
        comments_t(),
        t));
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_with_tags", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

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
  }

  SECTION("Validate data") {

    int num = sel->select_changesets({2});
    REQUIRE(num == 1);

    std::chrono::system_clock::time_point t = parse_time("2015-09-05T20:33:00Z");

    test_formatter f;
    sel->write_changesets(f, t);
    REQUIRE(f.m_changesets.size() == 1);

    tags_t tags;
    tags.emplace_back("test_key", "test_value");
    tags.emplace_back("test_key2", "test_value2");
    REQUIRE(
      f.m_changesets.front() ==
      test_formatter::changeset_t(
        changeset_info(
          2, // ID
          "2013-11-14T02:10:00Z", // created_at
          "2013-11-14T03:10:00Z", // closed_at
          1, // uid
          std::string("user_1"), // display_name
          {}, // bounding box
          1, // num_changes
          0 // comments_count
          ),
        tags,
        false,
        comments_t(),
        t));
  }
}


void check_changeset_with_comments_impl(
  data_selection& sel,
  bool include_discussion) {

  int num = sel.select_changesets({3});
  REQUIRE(num == 1);

  if (include_discussion) {
    sel.select_changeset_discussions();
  }

  std::chrono::system_clock::time_point t = parse_time("2015-09-05T20:38:00Z");

  test_formatter f;
  sel.write_changesets(f, t);
  REQUIRE(f.m_changesets.size() == 1);

  comments_t comments;
  {
    changeset_comment_info comment;
    comment.id = 1;
    comment.author_id = 3;
    comment.body = "a nice comment!";
    comment.created_at = "2015-09-05T20:37:01Z";
    comment.author_display_name = "user_3";
    comments.push_back(comment);
  }
  // note that we don't see the non-visible one in the database.
  REQUIRE(
    f.m_changesets.front() ==
    test_formatter::changeset_t(
      changeset_info(
        3, // ID
        "2013-11-14T02:10:00Z", // created_at
        "2013-11-14T03:10:00Z", // closed_at
        1, // uid
        std::string("user_1"), // display_name
        {}, // bounding box
        0, // num_changes
        1 // comments_count
        ),
      tags_t(),
      include_discussion,
      comments,
      t));
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_with_comments", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

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
  }

  SECTION("Check changeset without discussion") {
    REQUIRE_NOTHROW(check_changeset_with_comments_impl(*sel, false));
  }

  SECTION("Check changeset with discussion") {
    REQUIRE_NOTHROW(check_changeset_with_comments_impl(*sel, true));
  }
}

void init_changesets(test_database &tdb) {


  // Prepare users, changesets

  tdb.run_sql(R"(
	 INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
	 VALUES
           (1, 'user_1@example.com', 'x', null, '2013-11-14T02:10:00Z', 'user_1', true, 'confirmed'),
	   (31, 'demo@example.com', 'x', null, '2013-11-14T02:10:00Z', 'demo', true, 'confirmed'),
	   (32, 'user_2@example.com', '', '', '2013-11-14T02:10:00Z', 'user_2', false, 'active');

	INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
	VALUES
	  (51, 31, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
	  (52, 31, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 10000),
	  (53, 31, now() at time zone 'utc' - '12 hour' ::interval,
               now() at time zone 'utc' - '11 hour' ::interval, 10000),
	  (54, 32, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
	  (55, 32, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 0),
	  (56, 31, now() at time zone 'utc' - '23 hours' :: interval, now() at time zone 'utc' + '10 minutes' ::interval, 10000);

      INSERT INTO changeset_tags(changeset_id, k, v)
         VALUES
          (52, 'created_by', 'iD 4.0.3'),
          (52, 'comment', 'Adding some perfectly squared houses ;)');

      INSERT INTO user_blocks (user_id, creator_id, reason, ends_at, needs_view)
      VALUES (31,  32, '', now() at time zone 'utc' - ('1 hour' ::interval), false);

     INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at)
         VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c',
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');

      INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (67, 31, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');

      )"
  );

}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_create", "[changeset][db]" ) {


    const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
    const std::string generator = "Test";

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    SECTION("Initialize test data") {

      init_changesets(tdb);
    }

    SECTION("Unauthenticated user")
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
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 401);
    }

    SECTION("User providing wrong bearer token")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", "Bearer ZGVtbzppbnZhbGlkcGFzc3dvcmQK");
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
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 401);
    }

    SECTION("User is blocked (needs_view)")
    {
        tdb.run_sql(R"(UPDATE user_blocks SET needs_view = true where user_id = 31;)");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
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
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 403);

        tdb.run_sql(R"(UPDATE user_blocks SET needs_view = false where user_id = 31;)");
    }

    SECTION("User is blocked for 1 hour")
    {

        tdb.run_sql(R"(UPDATE user_blocks
                         SET needs_view = false,
                             ends_at = now() at time zone 'utc' + ('1 hour' ::interval)
                         WHERE user_id = 31;)");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
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
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());


	REQUIRE(req.response_status() == 403);

	tdb.run_sql(R"(UPDATE user_blocks
                          SET needs_view = false,
                              ends_at = now() at time zone 'utc' - ('1 hour' ::interval)
                          WHERE user_id = 31;)");
    }

    SECTION("Create new changeset")
    {
      // Set changeset sequence id to new start value

        tdb.run_sql(R"(  SELECT setval('changesets_id_seq', 500, false);  )");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="created_by" v="JOSM 1.61"/>
				<tag k="comment" v="Just adding some streetnames"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 200);
	REQUIRE(req.body().str() == "500");   // should have received changeset id 500

	auto sel = tdb.get_data_selection();

	int num = sel->select_changesets({500});
        REQUIRE(num == 1);

	std::chrono::system_clock::time_point t = std::chrono::system_clock::now();

	test_formatter f;
	sel->write_changesets(f, t);
	REQUIRE(f.m_changesets.size() == 1);

	tags_t tags;
	tags.emplace_back("comment", "Just adding some streetnames");
	tags.emplace_back("created_by", "JOSM 1.61");
	REQUIRE(
	  f.m_changesets.front() ==
	  test_formatter::changeset_t(
	    changeset_info(
              500, // ID
	      f.m_changesets.front().m_info.created_at, // created_at
	      f.m_changesets.front().m_info.closed_at, // closed_at
	      31, // uid
	      std::string("demo"), // display_name
	      {}, // bounding box
	      0, // num_changes
	      0 // comments_count
	      ),
	    tags,
	    false,
	    comments_t(),
	    t));


	// User 31 should have 1 changeset in total
	auto validate_cs_count = tdb.run_sql("SELECT * FROM users where id = 31 and changesets_count = 1");
	REQUIRE(validate_cs_count == 1); // found 1 matching row

	// Also user 31 should be subscribed to changeset 500
	auto validate_cs_subscribers = tdb.run_sql("SELECT * FROM changesets_subscribers where subscriber_id = 31 and changeset_id = 500");
	REQUIRE(validate_cs_subscribers == 1); // found 1 matching row
    }

}


TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_update", "[changeset][db]" ) {


    const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
    const std::string generator = "Test";

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    SECTION("Initialize test data") {
      init_changesets(tdb);
    }

    SECTION("unauthenticated user")
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
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 401); // should have received HTTP status 401 Unauthenticated

    }

    SECTION("wrong bearer token")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51");
	req.set_header("HTTP_AUTHORIZATION", "Bearer ZGVtbzppbnZhbGlkcGFzc3dvcmQK");
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
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 401);
    }


    SECTION("updating already closed changeset")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/53");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 409);
    }

    SECTION("updating non-existing changeset")
    {

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/666");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 404);
    }

    SECTION("changeset belongs to another user")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/54");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 409);
    }


    SECTION("Changeset which is open for 23 hours, and will close in 10 minutes")
    {
        // Expected result: "closed date - creation date" must be exactly 24 hours after update (assuming default settings)

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/56");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 200);

	int rows = tdb.run_sql(R"( select * from changesets where closed_at - created_at = '24 hours' ::interval and id = 56;  )");

	REQUIRE(rows == 1);   // Changeset 56 should be closed exactly 24 hours after creation

    }


    SECTION("Update changeset with 10k entries (may not fail)")
    {

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/52");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"( <osm>
			      <changeset>
				<tag k="tag1" v="value1"/>
				<tag k="tag2" v="value2"/>
				<tag k="tag3" v="value3"/>
			      </changeset>
			    </osm>  )" );

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 200);

	auto sel = tdb.get_data_selection();

	int num = sel->select_changesets({52});
	REQUIRE(num == 1);

	std::chrono::system_clock::time_point t = std::chrono::system_clock::now();

	test_formatter f;
	sel->write_changesets(f, t);
	REQUIRE(f.m_changesets.size() == 1);   // should have written one changeset 52.

	tags_t tags;
	tags.emplace_back("tag1", "value1");
	tags.emplace_back("tag2", "value2");
	tags.emplace_back("tag3", "value3");

	REQUIRE(
	  f.m_changesets.front() ==
	  test_formatter::changeset_t(
	    changeset_info(
	      52, // ID
	      f.m_changesets.front().m_info.created_at, // created_at
	      f.m_changesets.front().m_info.closed_at, // closed_at
	      31, // uid
	      std::string("demo"), // display_name
	      {}, // bounding box
	      10000, // num_changes
	      0 // comments_count
	      ),
	    tags,
	    false,
	    comments_t(),
	    t));
    }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "parallel test_changeset_update", "[changeset][db]" ) {


    const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
    const std::string generator = "Test";

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    SECTION("Initialize test data") {
      init_changesets(tdb);
    }

    SECTION("Try to update the same changeset in parallel") {

      test_request req{};
      RequestContext ctx{req};
      UserInfo user{};
      user.id = 31;
      ctx.user = user;

      auto factory = tdb.get_new_data_update_factory();
      auto txn = factory->get_default_transaction();
      auto upd = factory->make_data_update(*txn);
      auto cs_upd = upd->get_changeset_updater(ctx, 51);
      cs_upd->api_update_changeset({});


      // Try to update changeset in parallel thread while the main thread
      // is holding an exclusive lock on changeset 51
      auto future_cs = std::async(std::launch::async, [&ctx] {
         auto factory = tdb.get_new_data_update_factory();
         auto txn = factory->get_default_transaction();
         auto upd = factory->make_data_update(*txn);

         auto cs_upd = upd->get_changeset_updater(ctx, 51);
         cs_upd->api_update_changeset({});
         throw std::runtime_error("Should not reach this point");
      });

      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      upd->commit();

      REQUIRE_THROWS_MATCHES(future_cs.get(), http::conflict, Catch::Message("Changeset 51 is currently locked by another process."));
    }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_close", "[changeset][db]" ) {

    const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
    const std::string generator = "Test";

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    SECTION("Initialize test data") {
      init_changesets(tdb);
    }

    SECTION("unauthenticated user")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51/close");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 401);
    }

    SECTION("Close changeset")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/51/close");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 200);

    }

    SECTION("changeset already closed")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/53/close");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 409);
    }

    SECTION("updating non-existing changeset")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/666/close");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 404);
    }

    SECTION("changeset belongs to another user")
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "PUT");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/54/close");
	req.set_header("HTTP_AUTHORIZATION", bearertoken);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	// execute the request
	process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

	REQUIRE(req.response_status() == 409);
    }
}

int main(int argc, char *argv[]) {
  Catch::Session session;

  std::filesystem::path test_db_sql{ "test/structure.sql" };

  using namespace Catch::clara;
  auto cli =
      session.cli()
      | Opt(test_db_sql,
            "db-schema")    // bind variable to a new option, with a hint string
            ["--db-schema"] // the option names it will respond to
      ("test database schema file"); // description string for the help output

  session.cli(cli);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0)
    return returnCode;

  if (!test_db_sql.empty())
    DatabaseTestsFixture::setTestDatabaseSchema(test_db_sql);

  return session.run();
}
