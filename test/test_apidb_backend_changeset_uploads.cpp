/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include <chrono>
#include <cstdio>
#include <future>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <thread>
#include <fmt/core.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <sys/time.h>

#include "cgimap/options.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"
#include "cgimap/zlib.hpp"
#include "cgimap/request_context.hpp"
#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_xml_input_format.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

using Catch::Matchers::StartsWith;
using Catch::Matchers::EndsWith;
using Catch::Matchers::Equals;

class global_settings_enable_upload_rate_limiter_test_class : public global_settings_default {

public:
  // enable upload rate limiter
  bool get_ratelimiter_upload() const override { return true; }
};


class global_setting_enable_bbox_size_limiter_test_class : public global_settings_default {

public:
  // enable bbox size limiter
  bool get_bbox_size_limiter_upload() const override { return true; }
};

std::unique_ptr<xmlDoc, void (*)(xmlDoc *)> getDocument(const std::string &document)
{
  return {xmlReadDoc((xmlChar *)(document.c_str()), nullptr, nullptr, XML_PARSE_PEDANTIC | XML_PARSE_NONET), xmlFreeDoc};
}

std::optional<std::string> getXPath(xmlDoc* doc, const std::string& xpath)
{
  std::unique_ptr< xmlXPathContext, void (*)(xmlXPathContextPtr) > xpathCtx =
      { xmlXPathNewContext(doc), xmlXPathFreeContext };

  if (xpathCtx == nullptr)
    throw std::runtime_error("xpathCtx is null");

  std::unique_ptr< xmlXPathObject, void (*)(xmlXPathObjectPtr) > result =
     { xmlXPathEvalExpression((xmlChar*) xpath.c_str(), xpathCtx.get()),
      xmlXPathFreeObject };

  if (xmlXPathNodeSetIsEmpty(result->nodesetval))
    return {};

  const auto val = std::unique_ptr< xmlChar, decltype(xmlFree) >(xmlNodeGetContent(result->nodesetval->nodeTab[0]), xmlFree);
  return std::string((char*) val.get());
}


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

  void testRunStarting(Catch::TestRunInfo const& testRunInfo ) override {
    // load database schema when starting up tests
    tdb.setup(test_db_sql);
  }

  void testCaseStarting(Catch::TestCaseInfo const& testInfo ) override {
    tdb.testcase_starting();
  }

  void testCaseEnded(Catch::TestCaseStats const& testCaseStats ) override {
    tdb.testcase_ended();
  }
};

CATCH_REGISTER_LISTENER( CGImapListener )


std::string get_compressed_payload(std::string_view payload)
{
  std::stringstream body;
  std::stringstream output;

  // gzip compress payload
  test_output_buffer test_ob(output, body);
  zlib_output_buffer zlib_ob(test_ob, zlib_output_buffer::gzip);
  zlib_ob.write(payload.data(), payload.size());
  zlib_ob.close();

  return body.str();
}



TEST_CASE_METHOD( DatabaseTestsFixture, "test_single_nodes", "[changeset][upload][db]" ) {

  SECTION("Initialize test data") {

    tdb.run_sql(
        "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
        "VALUES "
        "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
        "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

        "INSERT INTO changesets (id, user_id, created_at, closed_at) "
        "VALUES "
        "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"
    );
  }

  static osm_nwr_id_t node_id;
  static osm_version_t node_version;

  test_request req{};
  RequestContext ctx{req};

  SECTION("Create new node")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
    node_updater->process_new_nodes();
    upd->commit();

    REQUIRE(change_tracking.created_node_ids.size() == 1);
    REQUIRE(change_tracking.created_node_ids[0].new_version == 1);
    REQUIRE(change_tracking.created_node_ids[0].old_id == -1);
    REQUIRE(change_tracking.created_node_ids[0].new_id > 0);

    node_id = change_tracking.created_node_ids[0].new_id;
    node_version = change_tracking.created_node_ids[0].new_version;

    {
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->check_node_visibility(node_id) == data_selection::exists);

      sel->select_nodes({ node_id });

      test_formatter f;
      sel->write_nodes(f);
      REQUIRE(f.m_nodes.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::node_t(
              element_info(node_id, 1, 1, f.m_nodes[0].elem.timestamp, 1, std::string("user_1"), true),
              131.0325171, -25.3448570,
              tags_t({{"name", "Uluṟu"}, {"ele", "863"}})
          ) == f.m_nodes[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }) == 1);

      test_formatter f2;
      sel->write_nodes(f2);
      REQUIRE(f2.m_nodes.size() == 1); // number of nodes written

      REQUIRE(
          test_formatter::node_t(
              element_info(node_id, 1, 1, f2.m_nodes[0].elem.timestamp, 1, std::string("user_1"), true),
              131.0325171, -25.3448570,
              tags_t({{"name", "Uluṟu"}, {"ele", "863"}})
          ) == f2.m_nodes[0]);
    }
  }

  SECTION("Create two nodes with the same old_id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->add_node(0, 0 , 1, -2, {});
    node_updater->add_node(10, 20 , 1, -2, {});
    REQUIRE_THROWS_MATCHES(node_updater->process_new_nodes(), http::bad_request,
        Catch::Message("Placeholder IDs must be unique for created elements."));
  }

  SECTION("Change existing node")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->modify_node(10, 20, 1, node_id, node_version, {});
    node_updater->process_modify_nodes();
    upd->commit();

    REQUIRE(change_tracking.modified_node_ids.size() == 1);
    REQUIRE (change_tracking.modified_node_ids[0].new_version == 2);
    REQUIRE(change_tracking.modified_node_ids[0].new_id == node_id);

    node_version = change_tracking.modified_node_ids[0].new_version;

    {
      // verify current tables
      auto sel = tdb.get_data_selection();

      sel->select_nodes({ node_id });

      test_formatter f;
      sel->write_nodes(f);
      REQUIRE(f.m_nodes.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::node_t(
              element_info(node_id, node_version, 1, f.m_nodes[0].elem.timestamp, 1, std::string("user_1"), true),
              20, 10,
              tags_t()
          ) == f.m_nodes[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }) == 2);

      test_formatter f2;
      sel->write_nodes(f2);
      REQUIRE(f2.m_nodes.size() == 2);

      REQUIRE(
          test_formatter::node_t(
              element_info(node_id, node_version, 1, f2.m_nodes[1].elem.timestamp, 1, std::string("user_1"), true),
              20, 10,
              tags_t()
          ) == f2.m_nodes[1]);
    }
  }

  SECTION("Change existing node with incorrect version number")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->modify_node(40, 50, 1, node_id, 666, {});
    REQUIRE_THROWS_MATCHES(node_updater->process_modify_nodes(), http::conflict,
        Catch::Message("Version mismatch: Provided 666, server had: 2 of Node 1"));
  }

  SECTION("Change existing node multiple times")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    int sign = -1;

    double minlat = 200.0;
    double minlon = 200.0;
    double maxlat = -200.0;
    double maxlon = -200.0;

    for (int i = 0; i < 10; i++) {
      sign = -sign;
      double lat = -5 * i * sign;
      double lon = 3 * i * sign;

      if (lat < minlat) minlat = lat;
      if (lon < minlon) minlon = lon;
      if (lat > maxlat) maxlat = lat;
      if (lon > maxlon) maxlon = lon;

      node_updater->modify_node(lat, lon, 1, node_id, node_version, {{"key", "value" + std::to_string(i)}});
      node_version++;
    }
    node_updater->process_modify_nodes();
    auto bbox = node_updater->bbox();
    auto bbox_expected = bbox_t(minlat, minlon, maxlat, maxlon);

    REQUIRE(bbox == bbox_expected);

    upd->commit();

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }) == 12); // number of nodes selected

      test_formatter f2;
      sel->write_nodes(f2);
      REQUIRE(f2.m_nodes.size() == node_version);

      REQUIRE(
          test_formatter::node_t(
              element_info(node_id, node_version, 1, f2.m_nodes[node_version - 1].elem.timestamp, 1, std::string("user_1"), true),
              -27, 45,
              tags_t({ {"key", "value9"} })
          ) == f2.m_nodes[node_version - 1]);

    }
  }

  SECTION("Delete existing node")
  {
    api06::OSMChange_Tracking change_tracking{};

    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_id, node_version, false);
    node_updater->process_delete_nodes();
    upd->commit();

    node_version++;

    REQUIRE(change_tracking.deleted_node_ids.size() == 1);
    REQUIRE(change_tracking.deleted_node_ids[0] == static_cast<osm_nwr_signed_id_t>(node_id));

    {
      // verify current tables
      auto sel = tdb.get_data_selection();
      REQUIRE(sel->check_node_visibility(node_id) == data_selection::deleted);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }) == node_version);

      test_formatter f2;
      sel->write_nodes(f2);
      REQUIRE(f2.m_nodes.size() == node_version);

      REQUIRE(
          test_formatter::node_t(
              element_info(node_id, node_version, 1, f2.m_nodes[node_version - 1].elem.timestamp, 1, std::string("user_1"), false),
              -27, 45,
              tags_t()
          ) == f2.m_nodes[node_version - 1]);

    }
  }

  SECTION("Try to delete already deleted node (if-unused not set)")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_id, node_version, false);
    REQUIRE_THROWS_AS(node_updater->process_delete_nodes(), http::gone);
  }

  SECTION("Try to delete already deleted node (if-unused set)")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_id, node_version, true);
    REQUIRE_NOTHROW(node_updater->process_delete_nodes());

    REQUIRE(change_tracking.skip_deleted_node_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_node_ids[0].new_version == node_version);
  }

  SECTION("Delete non-existing node")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, 424471234567890, 1, false);
    REQUIRE_THROWS_MATCHES(node_updater->process_delete_nodes(), http::not_found,
        Catch::Message("The following node ids are not known on the database: 424471234567890"));
  }

  SECTION("Modify non-existing node")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->modify_node(40, 50, 1, 4712334567890, 1, {});
    REQUIRE_THROWS_MATCHES(node_updater->process_modify_nodes(), http::not_found,
        Catch::Message("The following node ids are not known on the database: 4712334567890"));
  }

}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_single_ways", "[changeset][upload][db]" ) {

  static osm_nwr_id_t way_id;
  static osm_version_t way_version;
  static std::array<osm_nwr_id_t, 3> node_new_ids;

  test_request req{};
  RequestContext ctx{req};

  SECTION("Initialize test data") {

    tdb.run_sql(
        "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
        "VALUES "
        "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
        "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

        "INSERT INTO changesets (id, user_id, created_at, closed_at) "
        "VALUES "
        "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"
    );
  }

  SECTION("Create new way with two nodes")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
    node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
    node_updater->add_node(-25.34, 131.23, 1, -3, { });
    node_updater->process_new_nodes();

    way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
    way_updater->process_new_ways();

    upd->commit();

    REQUIRE(change_tracking.created_way_ids.size() == 1);
    REQUIRE(change_tracking.created_way_ids[0].new_version == 1);
    REQUIRE(change_tracking.created_way_ids[0].old_id == -1);
    REQUIRE(change_tracking.created_way_ids[0].new_id >= 1);

    way_id = change_tracking.created_way_ids[0].new_id;
    way_version = change_tracking.created_way_ids[0].new_version;

    for (const auto& id : change_tracking.created_node_ids) {
      node_new_ids[-1 * id.old_id - 1] = id.new_id;
    }

    {
      // verify current tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->check_way_visibility(way_id) == data_selection::exists);

      sel->select_ways({ way_id });

      test_formatter f;
      sel->write_ways(f);
      REQUIRE(f.m_ways.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::way_t(
              element_info(way_id, 1, 1, f.m_ways[0].elem.timestamp, 1, std::string("user_1"), true),
              nodes_t({node_new_ids[0], node_new_ids[1]}),
              tags_t({{"highway", "path"}})
          ) == f.m_ways[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_ways_with_history({ osm_nwr_id_t(way_id) }) == 1);

      test_formatter f2;
      sel->write_ways(f2);
      REQUIRE(f2.m_ways.size() == 1);

      REQUIRE(
          test_formatter::way_t(
              element_info(way_id, 1, 1, f2.m_ways[0].elem.timestamp, 1, std::string("user_1"), true),
              nodes_t({node_new_ids[0], node_new_ids[1]}),
              tags_t({{"highway", "path"}})
          ) == f2.m_ways[0]);
    }
  }

  SECTION("Create two ways with the same old_id must fail")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);
    auto way_updater = upd->get_way_updater(ctx, change_tracking);


    node_updater->add_node(0, 0 , 1, -1, {});
    node_updater->add_node(10, 20 , 1, -2, {});
    node_updater->process_new_nodes();

    way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
    way_updater->add_way(1, -1, { -2,  -1}, { {"highway", "path"}});
    REQUIRE_THROWS_MATCHES(way_updater->process_new_ways(), http::bad_request,
        Catch::Message("Placeholder IDs must be unique for created elements."));
  }

  SECTION("Create way with unknown placeholder ids")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->add_way(1, -1, { -1, -2}, { {"highway", "path"}});
    REQUIRE_THROWS_MATCHES(way_updater->process_new_ways(), http::bad_request,
        Catch::Message("Placeholder node not found for reference -1 in way -1"));
  }

  SECTION("Change existing way")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->modify_way(1, way_id, way_version,
        { static_cast<osm_nwr_signed_id_t>(node_new_ids[2]) },
        {{"access", "yes"}});
    way_updater->process_modify_ways();

    // Try to delete node in separate thread while new way version has't been committed yet
    // Shared lock on future way nodes blocks this activity.
    //
    // Note that shared locks on current_nodes table are also implicitly set due to the
    // foreign key relationship on the current_way_nodes table (current_way_nodes_node_id_fkey).

    auto future = std::async(std::launch::async, [&] {
       test_request req2{};
       RequestContext ctx2{req2};
       api06::OSMChange_Tracking change_tracking_2nd{};
       auto factory = tdb.get_new_data_update_factory();
       auto txn_2nd = factory->get_default_transaction();
       auto upd_2nd = factory->make_data_update(*txn_2nd);

       auto node_updater = upd_2nd->get_node_updater(ctx2, change_tracking_2nd);
       node_updater->delete_node(2, static_cast<osm_nwr_signed_id_t>(node_new_ids[2]), 1, false);
       // throws precondition_failed exception once the main process commits and releases the lock.
       node_updater->process_delete_nodes();
       upd_2nd->commit(); // not reached
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    upd->commit();

    REQUIRE_THROWS_MATCHES(future.get(), http::precondition_failed,
           Catch::Message(fmt::format("Precondition failed: Node {} is still used by ways 1.",node_new_ids[2])));

    REQUIRE(change_tracking.modified_way_ids.size() == 1);
    REQUIRE(change_tracking.modified_way_ids[0].new_version == 2);
    REQUIRE(change_tracking.modified_way_ids[0].new_id == way_id);

    way_version = change_tracking.modified_way_ids[0].new_version;

    {
      // verify current tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->check_node_visibility(static_cast<osm_nwr_signed_id_t>(node_new_ids[2])) == data_selection::exists);

      sel->select_ways({ way_id });

      test_formatter f;
      sel->write_ways(f);
      REQUIRE(f.m_ways.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::way_t(
              element_info(way_id, way_version, 1, f.m_ways[0].elem.timestamp, 1, std::string("user_1"), true),
              nodes_t({node_new_ids[2]}),
              tags_t({{"access", "yes"}})
          ) == f.m_ways[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_ways_with_history({ osm_nwr_id_t(way_id) }) == 2);

      test_formatter f2;
      sel->write_ways(f2);
      REQUIRE(f2.m_ways.size() == 2);

      REQUIRE(
          test_formatter::way_t(
              element_info(way_id, way_version, 1, f2.m_ways[1].elem.timestamp, 1, std::string("user_1"), true),
              nodes_t({node_new_ids[2]}),
              tags_t({{"access", "yes"}})
          ) == f2.m_ways[1]);
    }

  }

  SECTION("Change existing way with incorrect version number")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->modify_way(1, way_id, 666, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0])}, {});
    REQUIRE_THROWS_MATCHES(way_updater->process_modify_ways(), http::conflict,
        Catch::Message("Version mismatch: Provided 666, server had: 2 of Way 1"));
  }

  SECTION("Change existing way with incorrect version number and non-existing node id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->modify_way(1, way_id, 666, {5934531745}, {});
    REQUIRE_THROWS_MATCHES(way_updater->process_modify_ways(), http::conflict,
        Catch::Message("Version mismatch: Provided 666, server had: 2 of Way 1"));
  }

  SECTION("Change existing way with unknown node id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->modify_way(1, way_id, way_version, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), 9574853485634}, {});
    REQUIRE_THROWS_MATCHES(way_updater->process_modify_ways(), http::precondition_failed,
        Catch::Message("Precondition failed: Way 1 requires the nodes with id in 9574853485634, which either do not exist, or are not visible."));
  }

  SECTION("Change existing way with unknown placeholder node id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->modify_way(1, way_id, way_version, {-5}, {});
    REQUIRE_THROWS_MATCHES(way_updater->process_modify_ways(), http::bad_request,
        Catch::Message("Placeholder node not found for reference -5 in way 1"));
  }

  SECTION("TODO: Change existing way multiple times")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);




  }

  SECTION("Try to delete node which still belongs to way, if-unused not set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_new_ids[2], 1, false);
    REQUIRE_THROWS_MATCHES(node_updater->process_delete_nodes(), http::precondition_failed,
        Catch::Message(fmt::format("Precondition failed: Node {} is still used by ways 1.",node_new_ids[2])));
  }

  SECTION("Try to delete node which still belongs to way, if-unused set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_new_ids[2], 1, true);
    REQUIRE_NOTHROW(node_updater->process_delete_nodes());

    REQUIRE(change_tracking.skip_deleted_node_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_node_ids[0].new_version == 1);
  }

  SECTION("Delete existing way")
  {
    api06::OSMChange_Tracking change_tracking{};

    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->delete_way(1, way_id, way_version, false);
    way_updater->process_delete_ways();
    upd->commit();

    way_version++;

    REQUIRE(change_tracking.deleted_way_ids.size() == 1);
    REQUIRE(change_tracking.deleted_way_ids[0] == static_cast<osm_nwr_signed_id_t>(way_id));
    {
      auto sel = tdb.get_data_selection();
      REQUIRE(sel->check_way_visibility(way_id) == data_selection::deleted);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_ways_with_history({ osm_nwr_id_t(way_id) }) == way_version);

      test_formatter f2;
      sel->write_ways(f2);
      REQUIRE(f2.m_ways.size() == way_version);

      REQUIRE(
          test_formatter::way_t(
              element_info(way_id, way_version, 1, f2.m_ways[way_version - 1].elem.timestamp, 1, std::string("user_1"), false),
              nodes_t(),
              tags_t()
          ) == f2.m_ways[way_version - 1]);
    }
  }

  SECTION("Try to delete already deleted node (if-unused not set)")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->delete_way(1, way_id, way_version, false);
    REQUIRE_THROWS_MATCHES(way_updater->process_delete_ways(), http::gone,
        Catch::Message("The way with the id 1 has already been deleted"));
  }

  SECTION("Try to delete already deleted node (if-unused set)")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->delete_way(1, way_id, way_version, true);

    REQUIRE_NOTHROW(way_updater->process_delete_ways());
    REQUIRE(change_tracking.skip_deleted_way_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_way_ids[0].new_version == way_version);
  }

  SECTION("Delete non-existing way")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->delete_way(1, 424471234567890, 1, false);
    REQUIRE_THROWS_MATCHES(way_updater->process_delete_ways(), http::not_found,
        Catch::Message("The following way ids are unknown: 424471234567890"));
  }

  SECTION("Modify non-existing way")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->modify_way(1, 424471234567890, 1, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0])}, {});
    REQUIRE_THROWS_MATCHES(way_updater->process_modify_ways(), http::not_found,
        Catch::Message("The following way ids are unknown: 424471234567890"));
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_single_relations", "[changeset][upload][db]" ) {

  static osm_nwr_id_t relation_id;
  static osm_version_t relation_version;
  static std::array<osm_nwr_id_t, 3> node_new_ids;
  static osm_nwr_id_t way_new_id;

  static osm_nwr_id_t relation_id_1;
  static osm_version_t relation_version_1;
  static osm_nwr_id_t relation_id_2;
  static osm_version_t relation_version_2;

  test_request req{};
  RequestContext ctx{req};

  SECTION("Initialize test data") {

    tdb.run_sql(
        "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
        "VALUES "
        "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
        "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

        "INSERT INTO changesets (id, user_id, created_at, closed_at) "
        "VALUES "
        "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"
    );
  }


  SECTION("Create new relation with two nodes, and one way")
  {
    api06::OSMChange_Tracking change_tracking{};

    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);
    auto way_updater = upd->get_way_updater(ctx, change_tracking);
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
    node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
    // the following node is later used for a 'node still referenced by a relation' test
    node_updater->add_node( 15.5536221, 11.5462653,  1, -3, { });
    node_updater->process_new_nodes();

    way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
    way_updater->process_new_ways();

    // Remember new_ids for later tests. old_ids -1, -2, -3 are mapped to 0, 1, 2
    for (const auto& id : change_tracking.created_node_ids) {
      node_new_ids[-1 * id.old_id - 1] = id.new_id;
    }

    // Also remember the new_id for the way we are creating
    way_new_id = change_tracking.created_way_ids[0].new_id;

    rel_updater->add_relation(1, -1,
        {
            { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), "role1" },
            { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[1]), "role2" },
            { "Way",  static_cast<osm_nwr_signed_id_t>(way_new_id), "" }
        },
        {{"boundary", "administrative"}});

    rel_updater->process_new_relations();

    upd->commit();

    REQUIRE(change_tracking.created_relation_ids.size() == 1);
    REQUIRE(change_tracking.created_relation_ids[0].new_version == 1);
    REQUIRE(change_tracking.created_relation_ids[0].old_id == -1);
    REQUIRE(change_tracking.created_relation_ids[0].new_id >= 1);

    relation_id = change_tracking.created_relation_ids[0].new_id;
    relation_version = change_tracking.created_relation_ids[0].new_version;

    {
      // verify current tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->check_relation_visibility(relation_id) == data_selection::exists);

      sel->select_relations({relation_id });

      test_formatter f;
      sel->write_relations(f);
      REQUIRE(f.m_relations.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::relation_t(
              element_info(relation_id, 1, 1, f.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
              members_t(
                  {
        { element_type::node, node_new_ids[0], "role1" },
        { element_type::node, node_new_ids[1], "role2" },
        { element_type::way,  way_new_id, "" }
                  }
              ),
              tags_t({{"boundary", "administrative"}})
          ) == f.m_relations[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_relations_with_history({ osm_nwr_id_t(relation_id) }) == 1);

      test_formatter f2;
      sel->write_relations(f2);
      REQUIRE(f2.m_relations.size() == 1);

      REQUIRE(
          test_formatter::relation_t(
              element_info(relation_id, 1, 1, f2.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
              members_t(
                  {
        { element_type::node, node_new_ids[0], "role1" },
        { element_type::node, node_new_ids[1], "role2" },
        { element_type::way,  way_new_id, "" }
                  }
              ),
              tags_t({{"boundary", "administrative"}})
          ) == f2.m_relations[0]);
    }
  }


  SECTION("Create new relation with two nodes, and one way, only placeholder ids")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);
    auto way_updater = upd->get_way_updater(ctx, change_tracking);
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"} });
    node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
    node_updater->process_new_nodes();

    way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "track"}});
    way_updater->process_new_ways();

    rel_updater->add_relation(1, -1,
        {
            { "Node", -1, "role1" },
            { "Node", -2, "role2" },
            { "Way",  -1, "" }
        },
        {{"boundary", "administrative"}});

    rel_updater->process_new_relations();

    upd->commit();

    REQUIRE(change_tracking.created_relation_ids.size() == 1);
    REQUIRE(change_tracking.created_relation_ids[0].new_version == 1);
    REQUIRE(change_tracking.created_relation_ids[0].old_id == -1);
    REQUIRE(change_tracking.created_relation_ids[0].new_id >= 1);

    auto r_id = change_tracking.created_relation_ids[0].new_id;
    auto r_version = change_tracking.created_relation_ids[0].new_version;

    std::array<osm_nwr_id_t,2> n_new_ids;

    for (const auto& id : change_tracking.created_node_ids) {
      n_new_ids[-1 * id.old_id - 1] = id.new_id;
    }

    {
      // verify current tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->check_relation_visibility(r_id) == data_selection::exists);

      sel->select_relations({ r_id });

      test_formatter f;
      sel->write_relations(f);
      REQUIRE(f.m_relations.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::relation_t(
              element_info(r_id, r_version, 1, f.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
              members_t(
                  {
        { element_type::node, n_new_ids[0], "role1" },
        { element_type::node, n_new_ids[1], "role2" },
        { element_type::way,  change_tracking.created_way_ids[0].new_id, "" }
                  }
              ),
              tags_t({{"boundary", "administrative"}})
          ) == f.m_relations[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_relations_with_history({ osm_nwr_id_t( r_id ) }) == 1);

      test_formatter f2;
      sel->write_relations(f2);
      REQUIRE(f2.m_relations.size() == 1);

      REQUIRE(
          test_formatter::relation_t(
              element_info(r_id, r_version, 1, f2.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
              members_t(
                  {
        { element_type::node, n_new_ids[0], "role1" },
        { element_type::node, n_new_ids[1], "role2" },
        { element_type::way,  change_tracking.created_way_ids[0].new_id, "" }
                  }
              ),
              tags_t({{"boundary", "administrative"}})
          ) == f2.m_relations[0]);
    }

  }

  SECTION("Create two relations with the same old_id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, {}, {});
    rel_updater->add_relation(1, -1, {}, {{"key", "value"}});
    REQUIRE_THROWS_MATCHES(rel_updater->process_new_relations(), http::bad_request,
        Catch::Message("Placeholder IDs must be unique for created elements."));
  }

  SECTION("Create one relation with self reference")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, { { "Relation", -1, "role1" }}, {{"key1", "value1"}});
    REQUIRE_THROWS_MATCHES(rel_updater->process_new_relations(), http::bad_request,
        Catch::Message("Placeholder relation not found for reference -1 in relation -1"));
  }

  SECTION("Create two relations with references to each other")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, { { "Relation", -2, "role1" }}, {{"key1", "value1"}});
    rel_updater->add_relation(1, -2, { { "Relation", -1, "role2" }}, {{"key2", "value2"}});
    REQUIRE_THROWS_MATCHES(rel_updater->process_new_relations(), http::bad_request,
        Catch::Message("Placeholder relation not found for reference -2 in relation -1"));
  }

  SECTION("Create two relations with parent/child relationship")
  {
    api06::OSMChange_Tracking change_tracking{};

    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, { }, {{"key1", "value1"}});
    rel_updater->add_relation(1, -2, { { "Relation", -1, "role2" }}, {{"key2", "value2"}});
    REQUIRE_NOTHROW(rel_updater->process_new_relations());

    upd->commit();

    REQUIRE(change_tracking.created_relation_ids.size() == 2);

    relation_id_1 = change_tracking.created_relation_ids[0].new_id;
    relation_version_1 = change_tracking.created_relation_ids[0].new_version;

    relation_id_2 = change_tracking.created_relation_ids[1].new_id;
    relation_version_2 = change_tracking.created_relation_ids[1].new_version;

    {
      auto sel = tdb.get_data_selection();
      REQUIRE(sel->check_relation_visibility(relation_id_1) == data_selection::exists);
      REQUIRE(sel->check_relation_visibility(relation_id_2) == data_selection::exists);

      sel->select_relations({relation_id_1, relation_id_2});

      test_formatter f;
      sel->write_relations(f);
      REQUIRE(f.m_relations.size() == 2);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_relations_with_history({relation_id_1, relation_id_2}) == 2);

      test_formatter f2;
      sel->write_relations(f2);
      REQUIRE(f2.m_relations.size() == 2);
    }
  }

  SECTION("Create relation with unknown node placeholder id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, { { "Node", -10, "role1" }}, {{"key1", "value1"}});
    REQUIRE_THROWS_MATCHES(rel_updater->process_new_relations(), http::bad_request,
        Catch::Message("Placeholder node not found for reference -10 in relation -1"));
  }

  SECTION("Create relation with unknown way placeholder id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, { { "Way", -10, "role1" }}, {{"key1", "value1"}});
    REQUIRE_THROWS_MATCHES(rel_updater->process_new_relations(), http::bad_request,
        Catch::Message("Placeholder way not found for reference -10 in relation -1"));
  }

  SECTION("Create relation with unknown relation placeholder id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1, { { "Relation", -10, "role1" }}, {{"key1", "value1"}});
    REQUIRE_THROWS_MATCHES(rel_updater->process_new_relations(), http::bad_request,
        Catch::Message("Placeholder relation not found for reference -10 in relation -1"));
  }

  SECTION("Change existing relation")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version,
        {
            { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), "stop_position" },
            { "Way",  static_cast<osm_nwr_signed_id_t>(way_new_id), "outer" }
        },
        {{"admin_level", "4"}, {"boundary","administrative"}}
    );
    rel_updater->process_modify_relations();
    upd->commit();

    REQUIRE(change_tracking.modified_relation_ids.size() == 1);
    REQUIRE(change_tracking.modified_relation_ids[0].new_version == 2);
    REQUIRE(change_tracking.modified_relation_ids[0].new_id == relation_id);

    relation_version = change_tracking.modified_relation_ids[0].new_version;

    {
      // verify current tables
      auto sel = tdb.get_data_selection();
      sel->select_relations({ relation_id });

      test_formatter f;
      sel->write_relations(f);
      REQUIRE(f.m_relations.size() == 1);

      // we don't want to find out about deviating timestamps here...
      REQUIRE(
          test_formatter::relation_t(
              element_info(relation_id, relation_version, 1, f.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
              members_t(
                  {
        { element_type::node, node_new_ids[0], "stop_position" },
        { element_type::way,  way_new_id, "outer" }
                  }
              ),
              tags_t({{"admin_level", "4"}, {"boundary","administrative"}})
          ) == f.m_relations[0]);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_relations_with_history({ osm_nwr_id_t(relation_id) }) == 2);

      test_formatter f2;
      sel->write_relations(f2);
      REQUIRE(f2.m_relations.size() == 2);

      REQUIRE(
          test_formatter::relation_t(
              element_info(relation_id, relation_version, 1, f2.m_relations[1].elem.timestamp, 1, std::string("user_1"), true),
              members_t(
                  {
        { element_type::node, node_new_ids[0], "stop_position" },
        { element_type::way,  way_new_id, "outer" }
                  }
              ),
              tags_t({{"admin_level", "4"}, {"boundary","administrative"}})
          ) == f2.m_relations[1]);
    }
  }

  SECTION("Change existing relation with incorrect version number")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, 666,
        { {"Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::conflict,
        Catch::Message("Version mismatch: Provided 666, server had: 2 of Relation 1"));
  }

  SECTION("Change existing relation with incorrect version number and non-existing node id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, 666, { {"Node", 1434253485634, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::conflict,
        Catch::Message("Version mismatch: Provided 666, server had: 2 of Relation 1"));
  }

  SECTION("Change existing relation with unknown node id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version, { {"Node", 1434253485634, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::precondition_failed,
        Catch::Message("Precondition failed: Relation 1 requires the nodes with id in 1434253485634, which either do not exist, or are not visible."));
  }

  SECTION("Change existing relation with unknown way id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version, { {"Way", 9574853485634, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::precondition_failed,
        Catch::Message("Precondition failed: Relation 1 requires the ways with id in 9574853485634, which either do not exist, or are not visible."));
  }

  SECTION("Change existing relation with unknown relation id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version, { {"Relation", 9574853485634, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::precondition_failed,
        Catch::Message("Precondition failed: Relation 1 requires the relations with id in 9574853485634, which either do not exist, or are not visible."));
  }

  SECTION("Change existing relation with unknown node placeholder id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version, { {"Node", -10, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::bad_request,
        Catch::Message("Placeholder node not found for reference -10 in relation 1"));
  }

  SECTION("Change existing relation with unknown way placeholder id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version, { {"Way", -10, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::bad_request,
        Catch::Message("Placeholder way not found for reference -10 in relation 1"));
  }

  SECTION("Change existing relation with unknown relation placeholder id")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, relation_id, relation_version, { {"Relation", -10, ""} }, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::bad_request,
        Catch::Message("Placeholder relation not found for reference -10 in relation 1"));
  }

  SECTION("TODO: Change existing relation multiple times")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);



  }

  SECTION("Preparation for next test case: create a new relation with node_new_ids[2] as only member")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->add_relation(1, -1,
        {
            { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[2]), "center" }
        },
        {{"boundary", "administrative"}});

    rel_updater->process_new_relations();
    upd->commit();
  }

  SECTION("Try to delete node which still belongs to relation, if-unused not set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_new_ids[2], 1, false);
    REQUIRE_THROWS_MATCHES(node_updater->process_delete_nodes(), http::precondition_failed,
        Catch::Message(fmt::format("Precondition failed: Node {} is still used by relations 7.",node_new_ids[2])));
  }

  SECTION("Try to delete node which still belongs to relation, if-unused set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    node_updater->delete_node(1, node_new_ids[2], 1, true);
    REQUIRE_NOTHROW(node_updater->process_delete_nodes());

    REQUIRE(change_tracking.skip_deleted_node_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_node_ids[0].new_version == 1);
    REQUIRE(change_tracking.skip_deleted_node_ids[0].new_id == node_new_ids[2]);
  }

  SECTION("Try to delete way which still belongs to relation, if-unused not set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->delete_way(1, way_new_id, 1, false);
    REQUIRE_THROWS_MATCHES(way_updater->process_delete_ways(), http::precondition_failed,
        Catch::Message("Precondition failed: Way 3 is still used by relations 1."));
  }

  SECTION("Try to delete way which still belongs to relation, if-unused set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto way_updater = upd->get_way_updater(ctx, change_tracking);

    way_updater->delete_way(1, way_new_id, 1, true);
    REQUIRE_NOTHROW(way_updater->process_delete_ways());

    REQUIRE(change_tracking.skip_deleted_way_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_way_ids[0].new_version == 1);
    REQUIRE(change_tracking.skip_deleted_way_ids[0].new_id == way_new_id);
  }

  SECTION("Try to delete relation which still belongs to relation, if-unused not set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, relation_id_1, relation_version_1, false);
    REQUIRE_THROWS_MATCHES(rel_updater->process_delete_relations(), http::precondition_failed,
        Catch::Message("Precondition failed: The relation 3 is used in relations 4."));
  }

  SECTION("Try to delete relation which still belongs to relation, if-unused set")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, relation_id_1, relation_version_1, true);
    REQUIRE_NOTHROW(rel_updater->process_delete_relations());

    REQUIRE(change_tracking.skip_deleted_relation_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_relation_ids[0].new_version == 1);
    REQUIRE(change_tracking.skip_deleted_relation_ids[0].new_id == relation_id_1);
  }


  SECTION("Delete existing relation")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, relation_id, relation_version, false);
    rel_updater->process_delete_relations();
    upd->commit();

    relation_version++;

    REQUIRE(change_tracking.deleted_relation_ids.size() == 1);
    REQUIRE(change_tracking.deleted_relation_ids[0] == static_cast<osm_nwr_signed_id_t>(relation_id));

    {
      auto sel = tdb.get_data_selection();
      REQUIRE(sel->check_relation_visibility(relation_id) == data_selection::deleted);
    }

    {
      // verify historic tables
      auto sel = tdb.get_data_selection();

      REQUIRE(sel->select_relations_with_history({ osm_nwr_id_t(relation_id) }) == relation_version);

      test_formatter f2;
      sel->write_relations(f2);
      REQUIRE(f2.m_relations.size() == relation_version);

      REQUIRE(
          test_formatter::relation_t(
              element_info(relation_id, relation_version, 1, f2.m_relations[relation_version - 1].elem.timestamp, 1, std::string("user_1"), false),
              members_t(),
              tags_t()
          ) == f2.m_relations[relation_version - 1]);
    }
  }

  SECTION("Delete two relations with references to each other")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, relation_id_1, relation_version_1, false);
    rel_updater->delete_relation(1, relation_id_2, relation_version_2, false);
    rel_updater->process_delete_relations();
    upd->commit();

    REQUIRE(change_tracking.deleted_relation_ids.size() == 2);
    REQUIRE(sel->check_relation_visibility(relation_id_1) == data_selection::deleted);
    REQUIRE(sel->check_relation_visibility(relation_id_2) == data_selection::deleted);

    ++relation_version_1;
    ++relation_version_2;
  }

  SECTION("Revert deletion of two relations with master/child relationship")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);


    rel_updater->modify_relation(1, relation_id_1, relation_version_1,
          { {"Relation", static_cast<osm_nwr_signed_id_t>(relation_id_2), ""} }, {});
    rel_updater->modify_relation(1, relation_id_2, relation_version_2,
          {}, {});
    REQUIRE_NOTHROW(rel_updater->process_modify_relations());
    REQUIRE_NOTHROW(upd->commit());
  }

  SECTION("Try to delete already deleted relation (if-unused not set)")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, relation_id, relation_version, false);
    REQUIRE_THROWS_MATCHES(rel_updater->process_delete_relations(), http::gone,
        Catch::Message("The relation with the id 1 has already been deleted"));
  }

  SECTION("Try to delete already deleted relation (if-unused set)")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, relation_id, relation_version, true);
    REQUIRE_NOTHROW(rel_updater->process_delete_relations());

    REQUIRE(change_tracking.skip_deleted_relation_ids.size() == 1);
    REQUIRE(change_tracking.skip_deleted_relation_ids[0].new_version == relation_version);
  }

  SECTION("Delete non-existing relation")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->delete_relation(1, 424471234567890, 1, false);
    REQUIRE_THROWS_MATCHES(rel_updater->process_delete_relations(), http::not_found,
        Catch::Message("The following relation ids are unknown: 424471234567890"));
  }

  SECTION("Modify non-existing relation")
  {
    api06::OSMChange_Tracking change_tracking{};
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();
    auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

    rel_updater->modify_relation(1, 424471234567890, 1, {}, {});
    REQUIRE_THROWS_MATCHES(rel_updater->process_modify_relations(), http::not_found,
        Catch::Message("The following relation ids are unknown: 424471234567890"));
  }


  SECTION("Deleting child/parent in three level nested relations")
  {

    // Test case for https://github.com/zerebubuth/openstreetmap-cgimap/issues/223

    static osm_nwr_id_t relation_l3_id_1;
    static osm_version_t relation_l3_version_1;
    static osm_nwr_id_t relation_l3_id_2;
    static osm_version_t relation_l3_version_2;

    SECTION("Create three relations with grandparent/parent/child relationship")
    {
      api06::OSMChange_Tracking change_tracking{};

      auto upd = tdb.get_data_update();
      auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

      rel_updater->add_relation(1, -1, { }, {{"key1", "value1"}});
      rel_updater->add_relation(1, -2, { { "Relation", -1, "role2" }}, {{"key2", "value2"}});
      rel_updater->add_relation(1, -3, { { "Relation", -2, "role3" }}, {{"key3", "value3"}});
      REQUIRE_NOTHROW(rel_updater->process_new_relations());

      upd->commit();

      REQUIRE(change_tracking.created_relation_ids.size() == 3);

      relation_l3_id_1 = change_tracking.created_relation_ids[0].new_id;
      relation_l3_version_1 = change_tracking.created_relation_ids[0].new_version;

      relation_l3_id_2 = change_tracking.created_relation_ids[1].new_id;
      relation_l3_version_2 = change_tracking.created_relation_ids[1].new_version;

      // osm_nwr_id_t relation_l3_id_3 = change_tracking.created_relation_ids[2].new_id;
      // osm_version_t relation_l3_version_3 = change_tracking.created_relation_ids[2].new_version;
    }

    SECTION("Try to delete child/parent relations which still belong to grandparent relation, if-unused set")
    {
      api06::OSMChange_Tracking change_tracking{};
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

      rel_updater->delete_relation(1, relation_l3_id_1, relation_l3_version_1, true);
      rel_updater->delete_relation(1, relation_l3_id_2, relation_l3_version_2, true);
      REQUIRE_NOTHROW(rel_updater->process_delete_relations());

      REQUIRE(change_tracking.skip_deleted_relation_ids.size() == 2);
      REQUIRE(change_tracking.skip_deleted_relation_ids[0].new_version == 1);
      REQUIRE(change_tracking.skip_deleted_relation_ids[0].new_id == relation_l3_id_1);
      REQUIRE(change_tracking.skip_deleted_relation_ids[1].new_version == 1);
      REQUIRE(change_tracking.skip_deleted_relation_ids[1].new_id == relation_l3_id_2);
      REQUIRE(change_tracking.deleted_relation_ids.size() == 0);
    }
  }

  SECTION("Deletion relations, multilevel nested relations with dependency cycles")
  {

    /*
       Test case for https://github.com/zerebubuth/openstreetmap-cgimap/issues/223#issuecomment-617381115

       In this test case, we're checking that deleting relations -2, -3 and -4 is not possible, because they
       are directly (or indirectly) referenced by relation -1 as relation member.

       In addition, relations -2, -3 and -4 have a cyclic dependency. This way, we can test
       if the recursive relation member resolution in collect_recursive_relation_rel_member_ids
       works as expected.

          +----+     +----+
          | -1 | <-- | -2 | <+
          +----+     +----+  |
                       |     |
                       |     |
                       v     |
                     +----+  |
                     | -4 |  |
                     +----+  |
                       |     |
                       |     |
                       v     |
                     +----+  |
                     | -3 | -+
                     +----+

       "-1  <---- -2" means: relation -2 is a relation member of relation -1

     */

    static api06::OSMChange_Tracking change_tracking_1{};  // for step 1: create relations
    static api06::OSMChange_Tracking change_tracking_2{};  // for step 2: modify 1 relation
    static api06::OSMChange_Tracking change_tracking_3{};  // for step 3: delete 3 relations

    SECTION("Create multi-level relations")
    {
      auto upd = tdb.get_data_update();
      auto rel_updater = upd->get_relation_updater(ctx, change_tracking_1);

      // Note: we cannot add Relation -2 as a Relation member of relation -4 during creation,
      //       because relation -2 is not known at this point yet, and we don't allow forward
      //       references for Rails compatibility reasons.
      rel_updater->add_relation(1, -4, { }, {{"key4", "value4"}});
      rel_updater->add_relation(1, -3, { { "Relation", -4, "role4" }}, {{"key3", "value3"}});
      rel_updater->add_relation(1, -2, { { "Relation", -3, "role3" }}, {{"key2", "value2"}});
      rel_updater->add_relation(1, -1, { { "Relation", -2, "role2" }}, {{"key1", "value1"}});
      REQUIRE_NOTHROW(rel_updater->process_new_relations());

      upd->commit();

      REQUIRE(change_tracking_1.created_relation_ids.size() == 4);
      for (int i = 0; i < 4; i++) {
        REQUIRE(change_tracking_1.created_relation_ids[i].new_version == 1);
        REQUIRE(change_tracking_1.created_relation_ids[i].old_id == -4 + i);
        REQUIRE(change_tracking_1.created_relation_ids[i].new_id >= 1);
      }
    }

    SECTION("Change relation -4 by adding -2 as relation member (adds dependency loop)")
    {
      auto upd = tdb.get_data_update();
      auto rel_updater = upd->get_relation_updater(ctx, change_tracking_2);

      rel_updater->modify_relation(1,
                                   change_tracking_1.created_relation_ids[0].new_id,
                                   change_tracking_1.created_relation_ids[0].new_version,
                                   {
                                       { "Relation", static_cast<osm_nwr_signed_id_t>(change_tracking_1.created_relation_ids[2].new_id), "role2" }
                                   },
                                   {{"key2", "value2"}});

      REQUIRE_NOTHROW(rel_updater->process_modify_relations());
      REQUIRE_NOTHROW(upd->commit());

      REQUIRE(change_tracking_2.modified_relation_ids.size() == 1);
      REQUIRE(change_tracking_2.modified_relation_ids[0].new_id == change_tracking_1.created_relation_ids[0].new_id);
      REQUIRE(change_tracking_2.modified_relation_ids[0].new_version > change_tracking_1.created_relation_ids[0].new_version);
    }

    SECTION("Try to delete relations -2, -3 and -4, if-unused set")
    {
      auto upd = tdb.get_data_update();
      auto rel_updater = upd->get_relation_updater(ctx, change_tracking_3);

      // Delete relation -4
      rel_updater->delete_relation(1,
                                   change_tracking_2.modified_relation_ids[0].new_id,
                                   change_tracking_2.modified_relation_ids[0].new_version,
                                   true);

      // Delete relation -3
      rel_updater->delete_relation(1,
                                   change_tracking_1.created_relation_ids[1].new_id,
                                   change_tracking_1.created_relation_ids[1].new_version,
                                   true);

      // Delete relation -2
      rel_updater->delete_relation(1,
                                   change_tracking_1.created_relation_ids[2].new_id,
                                   change_tracking_1.created_relation_ids[2].new_version,
                                   true);

      REQUIRE_NOTHROW(rel_updater->process_delete_relations());
      REQUIRE_NOTHROW(upd->commit());

      REQUIRE(change_tracking_3.deleted_relation_ids.size() == 0);
      REQUIRE(change_tracking_3.skip_deleted_relation_ids.size() == 3);

      auto sel = tdb.get_data_selection();

      // check that there are no changes on the database, all 4 relations are all still visible
      for (int i = 0; i < 4; i++) {
        REQUIRE(sel->check_relation_visibility(static_cast<osm_nwr_signed_id_t>(change_tracking_1.created_relation_ids[i].new_id)) == data_selection::exists);
      }
    }
  }

  SECTION("Testing locking of future relation members")
  {
      // this test is checking that locking in ApiDB_Relation_Updater::lock_future_members is working as expected

      api06::OSMChange_Tracking change_tracking{};

      SECTION("Prepare data") {

        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(ctx, change_tracking);
        auto way_updater = upd->get_way_updater(ctx, change_tracking);
        auto rel_updater = upd->get_relation_updater(ctx, change_tracking);

        node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
        node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
        node_updater->add_node( 15.5536221, 11.5462653,  1, -3, { });
        node_updater->process_new_nodes();

        way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
        way_updater->process_new_ways();

        // Remember new_ids for later tests. old_ids -1, -2, -3 are mapped to 0, 1, 2
        for (const auto& id : change_tracking.created_node_ids) {
          node_new_ids[-1 * id.old_id - 1] = id.new_id;
        }

        // Also remember the new_id for the way we are creating
        way_new_id = change_tracking.created_way_ids[0].new_id;

        rel_updater->add_relation(1, -1,
            {
                { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), "role1" },
                { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[1]), "role2" }
            },
            {});

        rel_updater->process_new_relations();

        upd->commit();

        relation_id = change_tracking.created_relation_ids[0].new_id;
        relation_version = change_tracking.created_relation_ids[0].new_version;

      }

      SECTION("Create new relation") {

        api06::OSMChange_Tracking change_tracking_new_rel{};

        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(ctx, change_tracking_new_rel);

        rel_updater->add_relation(1, -1,
            {
                { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[2]), "role1" },
                { "Way",  static_cast<osm_nwr_signed_id_t>(way_new_id), "" },
                { "Relation", static_cast<osm_nwr_signed_id_t>(relation_id), "" }
            },
            {{"boundary", "administrative"}});

        rel_updater->process_new_relations();

        // Launch 3 threads, trying to delete future node/way/rel members of the new relation,
        // while the new relation hasn't been committed yet

        auto future_node = std::async(std::launch::async, [&] {
           test_request req2{};
           RequestContext ctx2{req2};
           api06::OSMChange_Tracking change_tracking_2nd{};
           auto factory = tdb.get_new_data_update_factory();
           auto txn_2nd = factory->get_default_transaction();
           auto upd_2nd = factory->make_data_update(*txn_2nd);

           auto node_updater = upd_2nd->get_node_updater(ctx2, change_tracking_2nd);
           node_updater->delete_node(2, static_cast<osm_nwr_signed_id_t>(node_new_ids[2]), 1, false);
           // throws precondition_failed exception once the main process commits and releases the lock.
           node_updater->process_delete_nodes();
           upd_2nd->commit(); // not reached
        });

        auto future_way = std::async(std::launch::async, [&] {
           test_request req3{};
           RequestContext ctx3{req3};
           api06::OSMChange_Tracking change_tracking_2nd{};
           auto factory = tdb.get_new_data_update_factory();
           auto txn_2nd = factory->get_default_transaction();
           auto upd_2nd = factory->make_data_update(*txn_2nd);

           auto way_updater = upd_2nd->get_way_updater(ctx3, change_tracking_2nd);
           way_updater->delete_way(2, static_cast<osm_nwr_signed_id_t>(way_new_id), 1, false);
           // throws precondition_failed exception once the main process commits and releases the lock.
           way_updater->process_delete_ways();
           upd_2nd->commit(); // not reached
        });

        auto future_rel = std::async(std::launch::async, [&] {
           test_request req4{};
           RequestContext ctx4{req4};
           api06::OSMChange_Tracking change_tracking_2nd{};
           auto factory = tdb.get_new_data_update_factory();
           auto txn_2nd = factory->get_default_transaction();
           auto upd_2nd = factory->make_data_update(*txn_2nd);

           auto rel_updater2 = upd_2nd->get_relation_updater(ctx4, change_tracking_2nd);
           rel_updater2->delete_relation(2, static_cast<osm_nwr_signed_id_t>(relation_id), 1, false);
           // throws precondition_failed exception once the main process commits and releases the lock.
           rel_updater2->process_delete_relations();
           upd_2nd->commit(); // not reached
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        upd->commit();

        auto new_rel_id = change_tracking_new_rel.created_relation_ids[0].new_id;

        {
          // verify current tables, all relation members, including the relation itself must be visible

          auto sel = tdb.get_data_selection();

          REQUIRE(sel->check_node_visibility(static_cast<osm_nwr_signed_id_t>(node_new_ids[2])) == data_selection::exists);
          REQUIRE(sel->check_way_visibility(static_cast<osm_nwr_signed_id_t>(way_new_id)) == data_selection::exists);
          REQUIRE(sel->check_relation_visibility(static_cast<osm_nwr_signed_id_t>(relation_id)) == data_selection::exists);
          REQUIRE(sel->check_relation_visibility(static_cast<osm_nwr_signed_id_t>(new_rel_id)) == data_selection::exists);

        }

        // Parallel attempts to delete future relation members must fail

        REQUIRE_THROWS_MATCHES(future_node.get(), http::precondition_failed,
               Catch::Message(fmt::format("Precondition failed: Node {} is still used by relations {}.",node_new_ids[2], new_rel_id)));

        REQUIRE_THROWS_MATCHES(future_way.get(), http::precondition_failed,
               Catch::Message(fmt::format("Precondition failed: Way {} is still used by relations {}.",way_new_id, new_rel_id)));


        REQUIRE_THROWS_MATCHES(future_rel.get(), http::precondition_failed,
               Catch::Message(fmt::format("Precondition failed: The relation {} is used in relations {}.",relation_id, new_rel_id)));


      }
  }
}


std::vector<api06::diffresult_t> process_payload(test_database &tdb, osm_changeset_id_t changeset, osm_user_id_t uid, const std::string& payload)
{
  auto sel = tdb.get_data_selection();
  auto upd = tdb.get_data_update();

  test_request req{};
  UserInfo user{ .id = uid };
  RequestContext ctx{ .req = req, .user = user};
  api06::OSMChange_Tracking change_tracking{};

  auto changeset_updater = upd->get_changeset_updater(ctx, changeset);
  auto node_updater = upd->get_node_updater(ctx, change_tracking);
  auto way_updater = upd->get_way_updater(ctx, change_tracking);
  auto relation_updater = upd->get_relation_updater(ctx, change_tracking);

  changeset_updater->lock_current_changeset(true);

  api06::OSMChange_Handler handler(*node_updater, *way_updater, *relation_updater, changeset);

  api06::OSMChangeXMLParser parser(handler);

  parser.process_message(payload);

  auto diffresult = change_tracking.assemble_diffresult();

  changeset_updater->update_changeset(handler.get_num_changes(),
      handler.get_bbox());

  upd->commit();

  return diffresult;
}



TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_update", "[changeset][upload][db]" ) {
  // C++20: switch to designated initializer for readability
  test_request req{};
  UserInfo user{};
  user.id = 1;
  RequestContext ctx{req};
  ctx.user = user;

  auto upd = tdb.get_data_update();
  auto changeset_updater = upd->get_changeset_updater(ctx, 1);

  SECTION("Initialize test data") {
    tdb.run_sql(
        "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
        "VALUES "
        "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
        "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

        "INSERT INTO changesets (id, user_id, created_at, closed_at) "
        "VALUES "
        "  (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval), "
        "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
        "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"
    );
  }

  SECTION("Trying to add CHANGESET_MAX_ELEMENTS to empty changeset - should succeed") {
    REQUIRE_NOTHROW(changeset_updater->update_changeset(global_settings::get_changeset_max_elements(), {}));  // use undefined bbox
  }

  SECTION("Trying to add CHANGESET_MAX_ELEMENTS + 1 to empty changeset - should fail") {
    REQUIRE_THROWS_AS(changeset_updater->update_changeset(global_settings::get_changeset_max_elements() + 1, {}), http::conflict);
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_osmchange_message", "[changeset][upload][db]" ) {

  SECTION("Initialize test data") {

  tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
      "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at) "
      "VALUES "
      "  (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval), "
      "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
      "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"
  );
  }


  SECTION("Test unknown changeset id") {

    REQUIRE_THROWS_MATCHES(process_payload(tdb, 1234, 1, R"(<?xml version="1.0" encoding="UTF-8"?>
        <osmChange version="0.6" generator="iD">
           <create>
              <node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1234">
                 <tag k="highway" v="bus_stop" />
              </node>
           </create>
        </osmChange>
      )"), http::not_found, Catch::Message(""));

  }

  // Test more complex examples, including XML parsing

  SECTION("Forward relation member declarations") {

    // Example from https://github.com/openstreetmap/iD/issues/3208#issuecomment-281942743
    // Relation id -3 has a relation member with forward reference to relation id -4

    REQUIRE_THROWS_MATCHES(process_payload(tdb, 1, 1, R"(<?xml version="1.0" encoding="UTF-8"?>
        <osmChange version="0.6" generator="iD">
           <create>
              <node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1">
                 <tag k="highway" v="bus_stop" />
              </node>
              <node id="-6" lon="11.62686047585252" lat="46.86730122861715" version="0" changeset="1">
                 <tag k="highway" v="bus_stop" />
              </node>
              <relation id="-2" version="0" changeset="1">
                 <member type="node" role="" ref="-5" />
                 <tag k="type" v="route" />
                 <tag k="name" v="AtoB" />
              </relation>
              <relation id="-3" version="0" changeset="1">
                 <member type="relation" role="" ref="-2" />
                 <member type="relation" role="" ref="-4" />
                 <tag k="type" v="route_master" />
                 <tag k="name" v="master" />
              </relation>
              <relation id="-4" version="0" changeset="1">
                 <member type="node" role="" ref="-6" />
                 <tag k="type" v="route" />
                 <tag k="name" v="BtoA" />
              </relation>
           </create>
           <modify />
           <delete if-unused="true" />
        </osmChange>

      )"), http::bad_request, Catch::Message("Placeholder relation not found for reference -4 in relation -3"));
  }

  SECTION("Testing correct parent/child sequence") {

    std::vector<api06::diffresult_t> diffresult;

    REQUIRE_NOTHROW(diffresult = process_payload(tdb, 1, 1, R"(<?xml version="1.0" encoding="UTF-8"?>
          <osmChange version="0.6" generator="iD">
             <create>
                <node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1">
                   <tag k="highway" v="bus_stop" />
                </node>
                <node id="-6" lon="11.62686047585252" lat="46.86730122861715" version="0" changeset="1">
                   <tag k="highway" v="bus_stop" />
                </node>
                <relation id="-2" version="0" changeset="1">
                   <member type="node" role="" ref="-5" />
                   <tag k="type" v="route" />
                   <tag k="name" v="AtoB" />
                </relation>
                <relation id="-3" version="0" changeset="1">
                   <member type="node" role="" ref="-6" />
                   <tag k="type" v="route" />
                   <tag k="name" v="BtoA" />
                </relation>
                <relation id="-4" version="0" changeset="1">
                   <member type="relation" role="" ref="-2" />
                   <member type="relation" role="" ref="-3" />
                   <tag k="type" v="route_master" />
                   <tag k="name" v="master" />
                </relation>
             </create>
             <modify />
             <delete if-unused="true" />
          </osmChange>

        )"));

    REQUIRE(diffresult.size() == 5);

    std::vector<osm_nwr_signed_id_t> old_ids{ -5, -6, -2, -3, -4};
    std::vector<object_type> obj_type{ object_type::node,
      object_type::node,
      object_type::relation,
      object_type::relation,
      object_type::relation };

    for (int i = 0; i < 5; i++) {
      REQUIRE(old_ids[i] == diffresult[i].old_id);
      REQUIRE(diffresult[i].new_version == 1);
      REQUIRE(static_cast<int>(obj_type[i]) == static_cast<int>(diffresult[i].obj_type));
      REQUIRE(static_cast<int>(operation::op_create) == static_cast<int>(diffresult[i].op));
      REQUIRE(false == diffresult[i].deletion_skipped);
    }
  }
}



TEST_CASE_METHOD( DatabaseTestsFixture, "test_osmchange_end_to_end", "[changeset][upload][db]" ) {

  const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
  const std::string generator = "Test";

  const std::optional<std::string> none{};

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  null_rate_limiter limiter;
  routes route;
  test_request req;

  req.set_header("REQUEST_METHOD", "POST");
  req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
  req.set_header("REMOTE_ADDR", "127.0.0.1");
  req.set_header("HTTP_AUTHORIZATION", bearertoken);

  // Prepare users, changesets
  SECTION("Initialize test data") {

    tdb.run_sql(R"(
         INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
         VALUES
           (1, 'demo@example.com', 'x', '', '2013-11-14T02:10:00Z', 'demo', true, 'confirmed'),
           (2, 'user_2@example.com', 'x', '', '2013-11-14T02:10:00Z', 'user_2', false, 'active');

        INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
        VALUES
          (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
          (2, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 10000),
          (3, 1, now() at time zone 'utc' - '12 hour' ::interval,
                 now() at time zone 'utc' - '11 hour' ::interval, 10000),
          (4, 2, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
          (5, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 0);

        INSERT INTO user_blocks (user_id, creator_id, reason, ends_at, needs_view)
        VALUES (1,  2, '', now() at time zone 'utc' - ('1 hour' ::interval), false);

        INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at)
         VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c',
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');

        INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (67, 1, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');
        )"
    );
  }

  SECTION("User providing wrong password"){

    // set up request headers from test case
    req.set_header("HTTP_AUTHORIZATION", "Bearer ZGVtbzppbnZhbGlkcGFzc3dvcmQK");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
           <osmChange version="0.6" generator="iD">
           <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="2"/></create>
           </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE(req.response_status() == 401);
  }

  SECTION("User is in status pending")
  {
    tdb.run_sql(R"(UPDATE users SET status = 'pending' where id = 1;)");

    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
           <osmChange version="0.6" generator="iD">
           <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/></create>
           </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    // User in status "pending" should return status HTTP 403
    REQUIRE(req.response_status() == 403);

    tdb.run_sql(R"(UPDATE users SET status = 'confirmed' where id = 1;)");
  }

  SECTION("User is blocked (needs_view)")
  {
    tdb.run_sql(R"(UPDATE user_blocks SET needs_view = true where user_id = 1;)");

    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
           <osmChange version="0.6" generator="iD">
           <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/></create>
           </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE(req.response_status() == 403);
    REQUIRE_THAT(req.body().str(), Equals("Your access to the API has been blocked. Please log-in to the web interface to find out more."));

    tdb.run_sql(R"(UPDATE user_blocks SET needs_view = false where user_id = 1;)");
  }

  SECTION("User is blocked for 1 hour")
  {
    tdb.run_sql(R"(UPDATE user_blocks
                       SET needs_view = false,
                           ends_at = now() at time zone 'utc' + ('1 hour' ::interval)
                       WHERE user_id = 1;)");

    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
           <osmChange version="0.6" generator="iD">
           <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/></create>
           </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE(req.response_status() == 403);
    REQUIRE_THAT(req.body().str(), Equals("Your access to the API has been blocked. Please log-in to the web interface to find out more."));

    tdb.run_sql(R"(UPDATE user_blocks
                        SET needs_view = false,
                            ends_at = now() at time zone 'utc' - ('1 hour' ::interval)
                        WHERE user_id = 1;)");
  }

  SECTION("Try to post a changeset, where the URL points to a different URL than the payload")
  {
    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
           <osmChange version="0.6" generator="iD">
           <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="2"/></create>
           </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE(req.response_status() == 409);
    REQUIRE_THAT(req.body().str(), Equals("Changeset mismatch: Provided 2 but only 1 is allowed"));
  }

  SECTION("Try to post a changeset, where the user doesn't own the changeset")
  {
    // set up request headers from test case
    req.set_header("REQUEST_URI", "/api/0.6/changeset/4/upload");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
           <osmChange version="0.6" generator="iD">
           <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="4"/></create>
           </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE(req.response_status() == 409);
    REQUIRE_THAT(req.body().str(), Equals("The user doesn't own that changeset"));
  }

  SECTION("Try to add a node to a changeset that already has 10000 elements (=max)")
  {
    // set up request headers from test case
    req.set_header("REQUEST_URI", "/api/0.6/changeset/2/upload");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                <osmChange version="0.6" generator="iD">
                   <create><node id="-5" lon="11" lat="46" version="0" changeset="2"/></create>
                </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE(req.response_status() == 409);
    REQUIRE_THAT(req.body().str(), StartsWith("The changeset 2 was closed at "));
  }

  SECTION("Try to add a node to a changeset that is already closed, X-Error-Format: XML error format response")
  {
    // set up request headers from test case
    req.set_header("REQUEST_URI", "/api/0.6/changeset/3/upload");
    // test x-error-format: xml http header
    req.set_header("HTTP_X_ERROR_FORMAT", "xml");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                <osmChange version="0.6" generator="iD">
                   <create><node id="-5" lon="11" lat="46" version="0" changeset="3"/></create>
                </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    REQUIRE_THAT(req.body().str(), StartsWith("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n<osmError>\r\n<status>409 Conflict</status>\r\n<message>The changeset 3 was closed"));
    REQUIRE_THAT(req.body().str(), EndsWith("</message>\r\n</osmError>\r\n"));
    // application_controller.rb, report_error sets http status to 200 instead of 409 in case of X-Format-Error format
    REQUIRE(req.response_status() == 200);
  }

  SECTION("Try to add a nodes, ways, relations to a changeset")
  {
    // Set sequences to new start values
    tdb.run_sql(R"(  SELECT setval('current_nodes_id_seq', 12000000000, false);
                       SELECT setval('current_ways_id_seq', 14000000000, false);
                       SELECT setval('current_relations_id_seq', 18000000000, false);
                   )");

    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                <osmChange version="0.6" generator="iD">
                <create>
                  <node id="-5" lon="11" lat="46" version="0" changeset="1">
                     <tag k="highway" v="bus_stop" />
                  </node>
                  <node id="-6" lon="13" lat="47" version="0" changeset="1">
                     <tag k="highway" v="bus_stop" />
                  </node>
                  <node id="-7" lon="-54" lat="12" version="0" changeset="1"/>
                  <way id="-10" version="0" changeset="1">
                    <nd ref="-5"/>
                    <nd ref="-6"/>
                  </way>
                  <way id="-11" version="0" changeset="1">
                    <nd ref="-6"/>
                    <nd ref="-7"/>
                  </way>
                  <relation id="-2" version="0" changeset="1">
                     <member type="node" role="" ref="-5" />
                     <tag k="type" v="route" />
                     <tag k="name" v="AtoB" />
                  </relation>
                  <relation id="-3" version="0" changeset="1">
                     <member type="node" role="" ref="-6" />
                     <tag k="type" v="route" />
                     <tag k="name" v="BtoA" />
                  </relation>
                  <relation id="-4" version="0" changeset="1">
                     <member type="relation" role="" ref="-2" />
                     <member type="relation" role="" ref="-3" />
                     <tag k="type" v="route_master" />
                     <tag k="name" v="master" />
                  </relation>
               </create>
               </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());

    REQUIRE(req.response_status() == 200);

    auto doc = getDocument(req.body().str());
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@old_id") == "-5");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_id") == "12000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@old_id") == "-6");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_id") == "12000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@old_id") == "-7");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@new_id") == "12000000002");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@old_id") == "-10");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@new_id") == "14000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/way[2]/@old_id") == "-11");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[2]/@new_id") == "14000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[2]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[1]/@old_id") == "-2");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[1]/@new_id") == "18000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[1]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[2]/@old_id") == "-3");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[2]/@new_id") == "18000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[2]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[3]/@old_id") == "-4");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[3]/@new_id") == "18000000002");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[3]/@new_version") == "1");

  }

  SECTION("Try to add, modify and delete nodes, ways, relations in changeset")
  {
    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                <osmChange version="0.6" generator="iD">
                <create>
                  <node id="-15" lon="4" lat="2" version="0" changeset="1"/>
                  <node id="-16" lon="3" lat="7" version="0" changeset="1"/>
                </create>
                <modify>
                  <node id="12000000000" lon="-11" lat="-46" version="1" changeset="1">
                     <tag k="highway" v="bus_stop" />
                     <tag k="name" v="Repubblica" />
                  </node>
                  <way id="14000000000" version="1" changeset="1">
                    <tag k="highway" v="residential"/>
                    <nd ref="-15"/>
                    <nd ref="-16"/>
                  </way>
                  <relation id="18000000000" version="1" changeset="1">
                     <tag k="type" v="route" />
                  </relation>
                  <relation id="18000000001" version="1" changeset="1">
                     <member type="way" role="test" ref="14000000000" />
                     <member type="node" role="" ref="12000000001" />
                     <member type="relation" role="bla" ref="18000000000" />
                     <tag k="type" v="route" />
                  </relation>
               </modify>
                <delete>
                  <relation id="18000000002" version="1" changeset="1"/>
                  <way id="14000000001" version="1" changeset="1"/>
                  <node id="12000000002" version="1" changeset="1"/>
                </delete>
                <delete if-unused="true">
                  <node id="12000000001" version="1" changeset="1"/>
                  <way id="14000000000" version="2" changeset="1"/>
                  <relation id="18000000000" version="2" changeset="1"/>
                </delete>
               </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());

    REQUIRE(req.response_status() == 200);

    auto doc = getDocument(req.body().str());
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@old_id") == "-15");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_id") == "12000000003");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@old_id") == "-16");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_id") == "12000000004");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@old_id") == "12000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@new_id") == "12000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@new_version") == "2");

    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@old_id") == "14000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@new_id") == "14000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@new_version") == "2");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[1]/@old_id") == "18000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[1]/@new_id") == "18000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[1]/@new_version") == "2");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[2]/@old_id") == "18000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[2]/@new_id") == "18000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[2]/@new_version") == "2");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[3]/@old_id") == "18000000002");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[3]/@new_id") == none);
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[3]/@new_version") == none);

    REQUIRE(getXPath(doc.get(), "/diffResult/way[2]/@old_id") == "14000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[2]/@new_id") == none);
    REQUIRE(getXPath(doc.get(), "/diffResult/way[2]/@new_version") == none);

    REQUIRE(getXPath(doc.get(), "/diffResult/node[4]/@old_id") == "12000000002");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[4]/@new_id") == none);
    REQUIRE(getXPath(doc.get(), "/diffResult/node[4]/@new_version") == none);

    REQUIRE(getXPath(doc.get(), "/diffResult/node[5]/@old_id") == "12000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[5]/@new_id") == "12000000001");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[5]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/way[3]/@old_id") == "14000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[3]/@new_id") == "14000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[3]/@new_version") == "2");

    REQUIRE(getXPath(doc.get(), "/diffResult/relation[4]/@old_id") == "18000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[4]/@new_id") == "18000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/relation[4]/@new_version") == "2");
  }

  SECTION("Multiple operations on the same node id -1")
  {
    // Set sequences to new start values
    tdb.run_sql(R"(  SELECT setval('current_nodes_id_seq', 13000000000, false);  )");

    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                <osmChange version="0.6" generator="iD">
                <create>
                   <node id="-1" lon="11.625506992810122" lat="46.866699181636555"  changeset="1">
                     <tag k="highway" v="bus_stop" />
                   </node>
                </create>
                <delete>
                   <node id="-1"  version="1" changeset="1" />
                </delete>
                <modify>
                   <node id="-1" lon="11.12" lat="46.13" version="2" changeset="1"/>
                </modify>
                <delete>
                    <node id="-1"  version="3" changeset="1" />
                </delete>
               </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());

    REQUIRE(req.response_status() == 200);

    auto doc = getDocument(req.body().str());
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@old_id") == "-1");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_id") == "13000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_version") == "1");

    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@old_id") == "-1");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_id") == none);
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_version") == none);

    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@old_id") == "-1");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@new_id") == "13000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[3]/@new_version") == "3");

    REQUIRE(getXPath(doc.get(), "/diffResult/node[4]/@old_id") == "-1");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[4]/@new_id") == none);
    REQUIRE(getXPath(doc.get(), "/diffResult/node[4]/@new_version") == none);

  }

  SECTION("Compressed upload")
  {
    std::string payload = R"(<?xml version="1.0" encoding="UTF-8"?>
        <osmChange version="0.6" generator="iD">
        <create>
          <node id="-5" lon="11" lat="46" version="0" changeset="1">
             <tag k="highway" v="bus_stop" />
          </node>
       </create>
       </osmChange>)";

    // set up request headers from test case
    req.set_header("HTTP_CONTENT_ENCODING", "gzip");

    req.set_payload(get_compressed_payload(payload));

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());

    REQUIRE(req.response_status() == 200);
  }

}


TEST_CASE_METHOD( DatabaseTestsFixture, "test_osmchange_rate_limiter", "[changeset][upload][db]" ) {

  // Upload rate limiter enabling
  auto test_settings = std::make_unique< global_settings_enable_upload_rate_limiter_test_class >();
  global_settings::set_configuration(std::move(test_settings));

  const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
  const std::string generator = "Test";

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  null_rate_limiter limiter;
  routes route;

  SECTION("Initialize test data") {

    tdb.run_sql(R"(
             INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
             VALUES
               (1, 'demo@example.com', 'xx', '', '2013-11-14T02:10:00Z', 'demo', true, 'confirmed'),
               (2, 'user_2@example.com', '', '', '2013-11-14T02:10:00Z', 'user_2', false, 'active');

            INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
            VALUES
              (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
              (3, 1, now() at time zone 'utc' - '12 hour' ::interval,
                     now() at time zone 'utc' - '11 hour' ::interval, 10000),
              (4, 2, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 10000),
              (5, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 10000);

            INSERT INTO user_blocks (user_id, creator_id, reason, ends_at, needs_view)
            VALUES (1,  2, '', now() at time zone 'utc' - ('1 hour' ::interval), false);

            SELECT setval('current_nodes_id_seq', 14000000000, false);

            INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at)
             VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c',
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');

            INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
              VALUES (67, 1, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');

            )"
    );

    // Test check_rate_limit database function.
    // User ids != 1 may not upload any changes,
    // User id may upload up to 99 changes
    // Real database function is managed outside of CGImap

    tdb.run_sql(R"(

          CREATE OR REPLACE FUNCTION api_rate_limit(user_id bigint)
            RETURNS integer
            AS $$
           DECLARE
             max_changes double precision;
            recent_changes int4;
          BEGIN
            IF user_id <> 1 THEN
              RETURN 0;
            ELSE
              max_changes = 99;
              SELECT COALESCE(SUM(changesets.num_changes), 0) INTO STRICT recent_changes FROM changesets
                 WHERE changesets.user_id = api_rate_limit.user_id
                   AND changesets.created_at >= CURRENT_TIMESTAMP AT TIME ZONE 'UTC' - '1 hour'::interval;

              RETURN max_changes - recent_changes;
            END IF;
          END;
          $$ LANGUAGE plpgsql STABLE;

        )");
  }

  SECTION("Try to upload a single change only")
  {
    // set up request headers from test case
    test_request req;
    req.set_header("REQUEST_METHOD", "POST");
    req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
    req.set_header("HTTP_AUTHORIZATION", bearertoken);
    req.set_header("REMOTE_ADDR", "127.0.0.1");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
               <osmChange version="0.6" generator="iD">
               <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/></create>
               </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());
    REQUIRE(req.response_status() == 200);

    auto doc = getDocument(req.body().str());
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@old_id") == "-5");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_id") == "14000000000");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_version") == "1");
  }

  SECTION("Try to upload 98 additional changes")
  {
    // We've already uploaded one change to changeset 1, we should be able
    // to upload 98 further changes before hitting the rate limit

    // This test checks that we're not counting any uncommitted changes
    // to the changeset table towards our quota.

    for (int nds = 100; nds > 97; nds--)
    {
      // set up request headers from test case
      test_request req;
      req.set_header("REQUEST_METHOD", "POST");
      req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
      req.set_header("HTTP_AUTHORIZATION", bearertoken);
      req.set_header("REMOTE_ADDR", "127.0.0.1");

      std::string nodes;

      for (int i=1; i <= nds; i++) {
        nodes += fmt::format(R"( <node id="{}" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/> )", -i);
      }

      req.set_payload(fmt::format(R"(<?xml version="1.0" encoding="UTF-8"?>
                 <osmChange version="0.6" generator="iD">
                 <create>{}</create>
                 </osmChange>)" , nodes));

      // execute the request
      process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

      if (nds > 98)
      {
        REQUIRE_THAT(req.body().str(), Equals("Upload has been blocked due to rate limiting. Please try again later."));
        REQUIRE(req.response_status() == 429);
      }
      else if (nds == 98)
      {
        CAPTURE(req.body().str());
        REQUIRE(req.response_status() == 200);

        auto doc = getDocument(req.body().str());
        for (int i = 1; i <= nds; i++) {
          REQUIRE(getXPath(doc.get(), fmt::format("/diffResult/node[{}]/@old_id", i)) == std::to_string(-i));
          REQUIRE(getXPath(doc.get(), fmt::format("/diffResult/node[{}]/@new_id", i)) == std::to_string(14000000199 + i));
          REQUIRE(getXPath(doc.get(), fmt::format("/diffResult/node[{}]/@new_version", i)) == "1");
        }
      }

    }
  }
}


TEST_CASE_METHOD( DatabaseTestsFixture, "test_osmchange_bbox_size_limiter", "[changeset][upload][db]" ) {

  // Upload bbox size limiter enabling
  auto test_settings = std::make_unique< global_setting_enable_bbox_size_limiter_test_class >();
  global_settings::set_configuration(std::move(test_settings));

  const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
  const std::string generator = "Test";

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  null_rate_limiter limiter;
  routes route;

  SECTION("Initialize test data") {

    tdb.run_sql(R"(
             INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
             VALUES
               (1, 'demo@example.com', 'xx', '', '2013-11-14T02:10:00Z', 'demo', true, 'confirmed');

            INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
            VALUES
              (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
              (3, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0);

            SELECT setval('current_nodes_id_seq', 14000000000, false);

            INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at)
             VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c',
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');

            INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
              VALUES (67, 1, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');

            )"
    );

    // Test api_size_limit database function.
    // Real database function is managed outside of CGImap

    tdb.run_sql(R"(

          CREATE OR REPLACE FUNCTION api_size_limit(user_id bigint)
            RETURNS bigint
            AS $$
          BEGIN
            RETURN 5000000;
          END;
          $$ LANGUAGE plpgsql STABLE;

        )");
  }

  SECTION("Try to upload one way with two nodes, with very large bbox")
  {
    // set up request headers from test case
    test_request req;
    req.set_header("REQUEST_METHOD", "POST");
    req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
    req.set_header("HTTP_AUTHORIZATION", bearertoken);
    req.set_header("REMOTE_ADDR", "127.0.0.1");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
               <osmChange version="0.6" generator="iD">
               <create>
                   <node id='-25355'  lat='68.13898255618' lon='-105.8206640625' changeset="1" />
                   <node id='-25357' lat='-34.30685345531' lon='80.8590234375' changeset="1" />
                   <way id='-579' changeset="1">
                     <nd ref='-25355' />
                     <nd ref='-25357' />
                   </way>
               </create>
               </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());
    REQUIRE(req.response_status() == 413);
  }

  SECTION("Try to upload twice in same changeset, two nodes with very large bbox")
  {
    // set up request headers from test case
    {
      test_request req;
      req.set_header("REQUEST_METHOD", "POST");
      req.set_header("REQUEST_URI", "/api/0.6/changeset/3/upload");
      req.set_header("HTTP_AUTHORIZATION", bearertoken);
      req.set_header("REMOTE_ADDR", "127.0.0.1");

      req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                 <osmChange version="0.6" generator="iD">
                 <create>
                     <node id='-25355'  lat='68.13898255618' lon='-105.8206640625' changeset="3" />
                 </create>
                 </osmChange>)" );

      // execute the request
      process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

      CAPTURE(req.body().str());
      REQUIRE(req.response_status() == 200);
    }

    {
      test_request req;
      req.set_header("REQUEST_METHOD", "POST");
      req.set_header("REQUEST_URI", "/api/0.6/changeset/3/upload");
      req.set_header("HTTP_AUTHORIZATION", bearertoken);
      req.set_header("REMOTE_ADDR", "127.0.0.1");

      req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                 <osmChange version="0.6" generator="iD">
                 <create>
                     <node id='-25357' lat='-34.30685345531' lon='80.8590234375' changeset="3" />
                 </create>
                 </osmChange>)" );

      // execute the request
      process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

      CAPTURE(req.body().str());
      REQUIRE(req.response_status() == 413);
    }
  }

  SECTION("Try to upload one way with two nodes, with very small bbox")
  {
    // set up request headers from test case
    test_request req;
    req.set_header("REQUEST_METHOD", "POST");
    req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
    req.set_header("HTTP_AUTHORIZATION", bearertoken);
    req.set_header("REMOTE_ADDR", "127.0.0.1");

    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
               <osmChange version="0.6" generator="iD">
               <create>
                   <node id='-25360' lat='51.50723246769' lon='-0.12171328202' changeset="1" />
                   <node id='-25361' lat='51.50719824397' lon='-0.12160197034' changeset="1" />
                   <way id='-582' changeset="1">
                      <nd ref='-25360' />
                      <nd ref='-25361' />
                   </way>
               </create>
               </osmChange>)" );

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());
    REQUIRE(req.response_status() == 200);

    auto doc = getDocument(req.body().str());
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@old_id") == "-25360");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@old_id") == "-25361");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@old_id") == "-582");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[1]/@new_version") == "1");
    REQUIRE(getXPath(doc.get(), "/diffResult/node[2]/@new_version") == "1");
    REQUIRE(getXPath(doc.get(), "/diffResult/way[1]/@new_version") == "1");
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
