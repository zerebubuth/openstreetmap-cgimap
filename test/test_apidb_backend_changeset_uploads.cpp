#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/program_options.hpp>

#include <sys/time.h>

#include "cgimap/config.hpp"
#include "cgimap/time.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"
#include "cgimap/output_buffer.hpp"

#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"

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

  void test_single_nodes(test_database &tdb) {
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

    osm_nwr_id_t node_id;
    osm_version_t node_version;

    // Create new node
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
      node_updater->process_new_nodes();
      upd->commit();

      if (change_tracking->created_node_ids.size() != 1)
	throw std::runtime_error("Expected 1 entry in created_node_ids");


      if (change_tracking->created_node_ids[0].new_version != 1)
	throw std::runtime_error("Expected new version == 1");

      if (change_tracking->created_node_ids[0].old_id != -1)
	throw std::runtime_error("Expected old_id == -1");

      if (change_tracking->created_node_ids[0].new_id < 1)
	throw std::runtime_error("Expected positive new_id");


      node_id = change_tracking->created_node_ids[0].new_id;
      node_version = change_tracking->created_node_ids[0].new_version;


      {
	auto sel = tdb.get_data_selection();

	if (sel->check_node_visibility(node_id) != data_selection::exists) {
	    throw std::runtime_error("Node should be visible, but isn't");
	}

	sel->select_nodes({ node_id });

	test_formatter f;
	sel->write_nodes(f);
	assert_equal<size_t>(f.m_nodes.size(), 1, "number of nodes written");

	// we don't want to find out about deviating timestamps here...
	assert_equal<test_formatter::node_t>(
	    test_formatter::node_t(
		element_info(node_id, 1, 1, f.m_nodes[0].elem.timestamp, 1, std::string("user_1"), true),
		131.0325171, -25.3448570,
		tags_t({{"name", "Uluṟu"}, {"ele", "863"}})
	    ),
	    f.m_nodes[0], "first node written");
      }

      {
	// verify historic tables
	auto sel = tdb.get_data_selection();

	assert_equal<int>(
	    sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }), 1,
	    "number of nodes selected");

	test_formatter f2;
	sel->write_nodes(f2);
	assert_equal<size_t>(f2.m_nodes.size(), 1, "number of nodes written");

	assert_equal<test_formatter::node_t>(
	    test_formatter::node_t(
		element_info(node_id, 1, 1, f2.m_nodes[0].elem.timestamp, 1, std::string("user_1"), true),
		131.0325171, -25.3448570,
		tags_t({{"name", "Uluṟu"}, {"ele", "863"}})
	    ),
	    f2.m_nodes[0], "first node written");
      }
    }

    // Create two nodes with the same old_id
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
        node_updater->add_node(0, 0 , 1, -2, {});
        node_updater->add_node(10, 20 , 1, -2, {});
        node_updater->process_new_nodes();
        throw std::runtime_error("Expected exception for duplicate old_ids");
      } catch (http::exception &e) {
	  if (e.code() != 400)
	    throw std::runtime_error("Expected HTTP 400 Bad request");
      }
    }

    // Change existing node
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      node_updater->modify_node(10, 20, 1, node_id, node_version, {});
      node_updater->process_modify_nodes();
      upd->commit();

      if (change_tracking->modified_node_ids.size() != 1)
	throw std::runtime_error("Expected 1 entry in modified_node_ids");


      if (change_tracking->modified_node_ids[0].new_version != 2)
	throw std::runtime_error("Expected new version == 2");

      if (change_tracking->modified_node_ids[0].new_id != node_id)
	throw std::runtime_error((boost::format("Expected new_id == node_id, %1%, %2%")
      % change_tracking->modified_node_ids[0].new_id
      % node_id).str());

      node_version = change_tracking->modified_node_ids[0].new_version;

      {
	// verify current tables
	auto sel = tdb.get_data_selection();

	sel->select_nodes({ node_id });

	test_formatter f;
	sel->write_nodes(f);
	assert_equal<size_t>(f.m_nodes.size(), 1, "number of nodes written");

	// we don't want to find out about deviating timestamps here...
	assert_equal<test_formatter::node_t>(
	    test_formatter::node_t(
		element_info(node_id, node_version, 1, f.m_nodes[0].elem.timestamp, 1, std::string("user_1"), true),
		20, 10,
		tags_t()
	    ),
	    f.m_nodes[0], "first node written");
      }

      {
	// verify historic tables
	auto sel = tdb.get_data_selection();

	assert_equal<int>(
	    sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }), 2,
	    "number of nodes selected");

	test_formatter f2;
	sel->write_nodes(f2);
	assert_equal<size_t>(f2.m_nodes.size(), 2, "number of nodes written");

	assert_equal<test_formatter::node_t>(
	    test_formatter::node_t(
		element_info(node_id, node_version, 1, f2.m_nodes[1].elem.timestamp, 1, std::string("user_1"), true),
		20, 10,
		tags_t()
	    ),
	    f2.m_nodes[1], "first node written");

      }
    }

    // Change existing node with incorrect version number
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
         node_updater->modify_node(40, 50, 1, node_id, 666, {});
         node_updater->process_modify_nodes();
	 throw std::runtime_error("Modifying a node with wrong version should raise http conflict error");
      } catch (http::exception &e) {
	  if (e.code() != 409)
	    throw std::runtime_error("Expected HTTP 409 Conflict");
      }
    }

    // Change existing node multiple times
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

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

         node_updater->modify_node(lat, lon, 1, node_id, node_version++, {{"key", "value" + std::to_string(i)}});
      }
      node_updater->process_modify_nodes();
      auto bbox = node_updater->bbox();
      auto bbox_expected = bbox_t(minlat, minlon, maxlat, maxlon);

      if (!(bbox == bbox_expected))
        throw std::runtime_error("Bbox does not match expected size");

      upd->commit();

      {
	// verify historic tables
	auto sel = tdb.get_data_selection();

	assert_equal<int>(
	    sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }), 12,
	    "number of nodes selected");

	test_formatter f2;
	sel->write_nodes(f2);
	assert_equal<size_t>(f2.m_nodes.size(), node_version, "number of nodes written");

	assert_equal<test_formatter::node_t>(
	    test_formatter::node_t(
		element_info(node_id, node_version, 1, f2.m_nodes[node_version - 1].elem.timestamp, 1, std::string("user_1"), true),
		-27, 45,
		tags_t({ {"key", "value9"} })
	    ),
	    f2.m_nodes[node_version - 1], "last node written");

      }

    }

    // Delete existing node
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();

      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      node_updater->delete_node(1, node_id, node_version++, false);
      node_updater->process_delete_nodes();
      upd->commit();

      if (change_tracking->deleted_node_ids.size() != 1)
	throw std::runtime_error("Expected 1 entry in deleted_node_ids");

      if (change_tracking->deleted_node_ids[0] != node_id) {
	  throw std::runtime_error("Expected node_id in deleted_node_ids");
      }

      {
	// verify current tables
        auto sel = tdb.get_data_selection();
        if (sel->check_node_visibility(node_id) != data_selection::deleted) {
	    throw std::runtime_error("Node should be deleted, but isn't");
        }
      }

      {
	// verify historic tables
	auto sel = tdb.get_data_selection();

	assert_equal<int>(
	    sel->select_nodes_with_history({ osm_nwr_id_t(node_id) }), node_version,
	    "number of nodes selected");

	test_formatter f2;
	sel->write_nodes(f2);
	assert_equal<size_t>(f2.m_nodes.size(), node_version, "number of nodes written");

	assert_equal<test_formatter::node_t>(
	    test_formatter::node_t(
		element_info(node_id, node_version, 1, f2.m_nodes[node_version - 1].elem.timestamp, 1, std::string("user_1"), false),
		-27, 45,
		tags_t()
	    ),
	    f2.m_nodes[node_version - 1], "first node written");

      }
    }

    // Try to delete already deleted node (if-unused not set)
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
	  node_updater->delete_node(1, node_id, node_version, false);
	  node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a deleted node should raise an http gone error");
      } catch (http::exception &e) {
	  if (e.code() != 410)
	    throw std::runtime_error("Expected HTTP 410 Gone");
      }
    }

    // Try to delete already deleted node (if-unused set)
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
	  node_updater->delete_node(1, node_id, node_version, true);
	  node_updater->process_delete_nodes();
      } catch (http::exception& e) {
	  throw std::runtime_error("HTTP Exception unexpected");
      }

      if (change_tracking->skip_deleted_node_ids.size() != 1)
	throw std::runtime_error("Expected 1 entry in skip_deleted_node_ids");

      if (change_tracking->skip_deleted_node_ids[0].new_version != node_version)
	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_node_ids")
                                 % node_version).str());
    }

    // Delete non-existing node
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
	  node_updater->delete_node(1, 424471234567890, 1, false);
	  node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a non-existing node should raise an http not found error");
      } catch (http::exception &e) {
	  if (e.code() != 404)
	    throw std::runtime_error("Expected HTTP 404 Not found");
      }
    }

    // Modify non-existing node
    {
      auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
         node_updater->modify_node(40, 50, 1, 4712334567890, 1, {});
         node_updater->process_modify_nodes();
	 throw std::runtime_error("Modifying a non-existing node should trigger a not found error");
      } catch (http::exception &e) {
	  if (e.code() != 404)
	    throw std::runtime_error("Expected HTTP 404 Not found");
      }
    }

  }

  void test_single_ways(test_database &tdb) {
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

      osm_nwr_id_t way_id;
      osm_version_t way_version;
      osm_nwr_id_t node_new_ids[2];

      // Create new way with two nodes
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);

        node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
        node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
        node_updater->process_new_nodes();

        way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
        way_updater->process_new_ways();

        upd->commit();

        if (change_tracking->created_way_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in created_way_ids");


        if (change_tracking->created_way_ids[0].new_version != 1)
  	throw std::runtime_error("Expected new version == 1");

        if (change_tracking->created_way_ids[0].old_id != -1)
  	throw std::runtime_error("Expected old_id == -1");

        if (change_tracking->created_way_ids[0].new_id < 1)
  	throw std::runtime_error("Expected positive new_id");

        way_id = change_tracking->created_way_ids[0].new_id;
        way_version = change_tracking->created_way_ids[0].new_version;

        for (const auto id : change_tracking->created_node_ids) {
           node_new_ids[-1 * id.old_id - 1] = id.new_id;
        }

        {
          // verify current tables
          auto sel = tdb.get_data_selection();

          if (sel->check_way_visibility(way_id) != data_selection::exists) {
              throw std::runtime_error("Way should be visible, but isn't");
          }

          sel->select_ways({ way_id });

          test_formatter f;
          sel->write_ways(f);
          assert_equal<size_t>(f.m_ways.size(), 1, "number of ways written");

          // we don't want to find out about deviating timestamps here...
          assert_equal<test_formatter::way_t>(
              test_formatter::way_t(
        	  element_info(way_id, 1, 1, f.m_ways[0].elem.timestamp, 1, std::string("user_1"), true),
		  nodes_t({node_new_ids[0], node_new_ids[1]}),
		  tags_t({{"highway", "path"}})
              ),
	      f.m_ways[0], "first way written");

        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_ways_with_history({ osm_nwr_id_t(way_id) }), 1,
	      "number of ways selected");

          test_formatter f2;
          sel->write_ways(f2);
          assert_equal<size_t>(f2.m_ways.size(), 1, "number of ways written");

          assert_equal<test_formatter::way_t>(
              test_formatter::way_t(
        	  element_info(way_id, 1, 1, f2.m_ways[0].elem.timestamp, 1, std::string("user_1"), true),
		  nodes_t({node_new_ids[0], node_new_ids[1]}),
		  tags_t({{"highway", "path"}})
              ),
	      f2.m_ways[0], "first way written");
        }
      }


      // Create two ways with the same old_id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          node_updater->add_node(0, 0 , 1, -1, {});
          node_updater->add_node(10, 20 , 1, -2, {});
          node_updater->process_new_nodes();

          way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
          way_updater->add_way(1, -1, { -2,  -1}, { {"highway", "path"}});
          way_updater->process_new_ways();

          throw std::runtime_error("Expected exception for duplicate old_ids");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP 400 Bad request");
        }
      }

      // Create way with unknown placeholder ids
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          way_updater->add_way(1, -1, { -1, -2}, { {"highway", "path"}});
          way_updater->process_new_ways();

          throw std::runtime_error("Expected exception for unknown placeholder ids");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP 400 Bad request");
        }
      }

      // Change existing way
     {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        way_updater->modify_way(1, way_id, way_version,
				{ static_cast<osm_nwr_signed_id_t>(node_new_ids[0]) },
				{{"access", "yes"}});
        way_updater->process_modify_ways();
        upd->commit();

        if (change_tracking->modified_way_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in modified_way_ids");


        if (change_tracking->modified_way_ids[0].new_version != 2)
  	throw std::runtime_error("Expected new version == 2");

        if (change_tracking->modified_way_ids[0].new_id != way_id)
  	throw std::runtime_error((boost::format("Expected new_id == way_id, %1%, %2%")
                                   % change_tracking->modified_way_ids[0].new_id
  				 % way_id).str());

        way_version = change_tracking->modified_way_ids[0].new_version;

        {
          // verify current tables
          auto sel = tdb.get_data_selection();

          sel->select_ways({ way_id });

          test_formatter f;
          sel->write_ways(f);
          assert_equal<size_t>(f.m_ways.size(), 1, "number of ways written");

          // we don't want to find out about deviating timestamps here...
          assert_equal<test_formatter::way_t>(
              test_formatter::way_t(
        	  element_info(way_id, way_version, 1, f.m_ways[0].elem.timestamp, 1, std::string("user_1"), true),
		  nodes_t({node_new_ids[0]}),
		  tags_t({{"access", "yes"}})
              ),
	      f.m_ways[0], "second way written");
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_ways_with_history({ osm_nwr_id_t(way_id) }), 2,
	      "number of ways selected");

          test_formatter f2;
          sel->write_ways(f2);
          assert_equal<size_t>(f2.m_ways.size(), 2, "number of ways written");

          assert_equal<test_formatter::way_t>(
              test_formatter::way_t(
        	  element_info(way_id, way_version, 1, f2.m_ways[1].elem.timestamp, 1, std::string("user_1"), true),
		  nodes_t({node_new_ids[0]}),
		  tags_t({{"access", "yes"}})
              ),
	      f2.m_ways[1], "second way written");
        }

      }

      // Change existing way with incorrect version number
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, 666, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0])}, {});
           way_updater->process_modify_ways();
  	 throw std::runtime_error("Modifying a way with wrong version should raise http conflict error");
        } catch (http::exception &e) {
  	  if (e.code() != 409)
  	    throw std::runtime_error("Expected HTTP 409 Conflict");
        }
      }

      // Change existing way with incorrect version number and non-existing node id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, 666, {static_cast<osm_nwr_signed_id_t>(5934531745)}, {});
           way_updater->process_modify_ways();
  	 throw std::runtime_error("Modifying a way with wrong version and non-existing node id should raise http conflict error");
        } catch (http::exception &e) {
  	  if (e.code() != 409)
  	    throw std::runtime_error("Expected HTTP 409 Conflict");
        }
      }

      // Change existing way with unknown node id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, way_version, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), 9574853485634}, {});
           way_updater->process_modify_ways();
           throw std::runtime_error("Modifying a way with unknown node id should raise http precondition failed error");
        } catch (http::exception &e) {
            if (e.code() != 412)
              throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Change existing way with unknown placeholder node id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, way_version, {-5}, {});
           way_updater->process_modify_ways();
           throw std::runtime_error("Modifying a way with unknown placeholder node id should raise http bad request");
        } catch (http::exception &e) {
            if (e.code() != 400)
              throw std::runtime_error("Expected HTTP 400 bad request");
        }
      }

      // TODO: Change existing way multiple times
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);




      }

      // Try to delete node which still belongs to way, if-unused not set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);

        try {
          node_updater->delete_node(1, node_new_ids[0], 1, false);
          node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a node that is still referenced by way should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Try to delete node which still belongs to way, if-unused set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);

        try {
          node_updater->delete_node(1, node_new_ids[0], 1, true);
          node_updater->process_delete_nodes();
        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        if (change_tracking->skip_deleted_node_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in skip_deleted_node_ids");

        if (change_tracking->skip_deleted_node_ids[0].new_version != 1)
  	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_node_ids")
                                   % 1).str());

      }

      // Delete existing way
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();

        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        way_updater->delete_way(1, way_id, way_version++, false);
        way_updater->process_delete_ways();
        upd->commit();

        if (change_tracking->deleted_way_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in deleted_way_ids");

        if (change_tracking->deleted_way_ids[0] != way_id) {
  	  throw std::runtime_error("Expected way_id in deleted_way_ids");
        }
        {
          auto sel = tdb.get_data_selection();
          if (sel->check_way_visibility(way_id) != data_selection::deleted) {
              throw std::runtime_error("Way should be deleted, but isn't");
          }
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_ways_with_history({ osm_nwr_id_t(way_id) }), way_version,
	      "number of ways selected");

          test_formatter f2;
          sel->write_ways(f2);
          assert_equal<size_t>(f2.m_ways.size(), way_version, "number of ways written");

          assert_equal<test_formatter::way_t>(
              test_formatter::way_t(
        	  element_info(way_id, way_version, 1, f2.m_ways[way_version - 1].elem.timestamp, 1, std::string("user_1"), false),
		  nodes_t(),
		  tags_t()
              ),
	      f2.m_ways[way_version - 1], "deleted way written");
        }
      }

      // Try to delete already deleted node (if-unused not set)
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
  	  way_updater->delete_way(1, way_id, way_version, false);
  	  way_updater->process_delete_ways();
  	  throw std::runtime_error("Deleting a non-existing way should raise an http gone error");
        } catch (http::exception &e) {
  	  if (e.code() != 410)
  	    throw std::runtime_error("Expected HTTP 410 Gone");
        }
      }

      // Try to delete already deleted node (if-unused set)
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
  	  way_updater->delete_way(1, way_id, way_version, true);
  	  way_updater->process_delete_ways();
        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        if (change_tracking->skip_deleted_way_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in skip_deleted_way_ids");

        if (change_tracking->skip_deleted_way_ids[0].new_version != way_version)
  	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_way_ids")
                                   % way_version).str());
      }

      // Delete non-existing way
      {
        auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          way_updater->delete_way(1, 424471234567890, 1, false);
          way_updater->process_delete_ways();
  	  throw std::runtime_error("Deleting a non-existing way should raise an http not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP 404 Not found");
        }
      }

      // Modify non-existing way
      {
        auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, 424471234567890, 1, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0])}, {});
           way_updater->process_modify_ways();
  	 throw std::runtime_error("Modifying a non-existing way should trigger a not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP 404 Not found");
        }
      }
    }

  void test_single_relations(test_database &tdb) {
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

      osm_nwr_id_t relation_id;
      osm_version_t relation_version;
      osm_nwr_id_t node_new_ids[3];
      osm_nwr_id_t way_new_id;

      osm_nwr_id_t relation_id_1;
      osm_version_t relation_version_1;
      osm_nwr_id_t relation_id_2;
      osm_version_t relation_version_2;

      // Create new relation with two nodes, and one way
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();

        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

        node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
        node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
        // the following node is later used for a 'node still referenced by a relation' test
        node_updater->add_node( 15.5536221, 11.5462653,  1, -3, { });
        node_updater->process_new_nodes();

        way_updater->add_way(1, -1, { -1,  -2}, { {"highway", "path"}});
        way_updater->process_new_ways();

        // Remember new_ids for later tests. old_ids -1, -2, -3 are mapped to 0, 1, 2
        for (const auto id : change_tracking->created_node_ids) {
           node_new_ids[-1 * id.old_id - 1] = id.new_id;
        }

        // Also remember the new_id for the way we are creating
        way_new_id = change_tracking->created_way_ids[0].new_id;

        rel_updater->add_relation(1, -1,
	  {
	      { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), "role1" },
	      { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[1]), "role2" },
	      { "Way",  static_cast<osm_nwr_signed_id_t>(way_new_id), "" }
	  },
	  {{"boundary", "administrative"}});

        rel_updater->process_new_relations();

        upd->commit();

        if (change_tracking->created_relation_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in created_relation_ids");


        if (change_tracking->created_relation_ids[0].new_version != 1)
  	throw std::runtime_error("Expected new version == 1");

        if (change_tracking->created_relation_ids[0].old_id != -1)
  	throw std::runtime_error("Expected old_id == -1");

        if (change_tracking->created_relation_ids[0].new_id < 1)
  	throw std::runtime_error("Expected positive new_id");

        relation_id = change_tracking->created_relation_ids[0].new_id;
        relation_version = change_tracking->created_relation_ids[0].new_version;

        {
          // verify current tables
          auto sel = tdb.get_data_selection();

          if (sel->check_relation_visibility(relation_id) != data_selection::exists) {
              throw std::runtime_error("Relation should be visible, but isn't");
          }

          sel->select_relations({relation_id });

          test_formatter f;
          sel->write_relations(f);
          assert_equal<size_t>(f.m_relations.size(), 1, "number of relations written");

          // we don't want to find out about deviating timestamps here...
          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(relation_id, 1, 1, f.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
		  members_t(
		      {
            { element_type_node, node_new_ids[0], "role1" },
	    { element_type_node, node_new_ids[1], "role2" },
	    { element_type_way,  way_new_id, "" }
		      }
		  ),
		  tags_t({{"boundary", "administrative"}})
              ),
	      f.m_relations[0], "first relation written");
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_relations_with_history({ osm_nwr_id_t(relation_id) }), 1,
	      "number of relations selected");

          test_formatter f2;
          sel->write_relations(f2);
          assert_equal<size_t>(f2.m_relations.size(), 1, "number of relations written");

          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(relation_id, 1, 1, f2.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
		  members_t(
		      {
            { element_type_node, node_new_ids[0], "role1" },
	    { element_type_node, node_new_ids[1], "role2" },
	    { element_type_way,  way_new_id, "" }
		      }
		  ),
		  tags_t({{"boundary", "administrative"}})
              ),
	      f2.m_relations[0], "first relation written");
        }
      }


      // Create new relation with two nodes, and one way, only placeholder ids
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

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

        if (change_tracking->created_relation_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in created_relation_ids");

        if (change_tracking->created_relation_ids[0].new_version != 1)
  	throw std::runtime_error("Expected new version == 1");

        if (change_tracking->created_relation_ids[0].old_id != -1)
  	throw std::runtime_error("Expected old_id == -1");

        if (change_tracking->created_relation_ids[0].new_id < 1)
  	throw std::runtime_error("Expected positive new_id");

        auto r_id = change_tracking->created_relation_ids[0].new_id;
        auto r_version = change_tracking->created_relation_ids[0].new_version;

        osm_nwr_id_t n_new_ids[2];

        for (const auto id : change_tracking->created_node_ids) {
           n_new_ids[-1 * id.old_id - 1] = id.new_id;
        }

        {
          // verify current tables
          auto sel = tdb.get_data_selection();

          if (sel->check_relation_visibility(r_id) != data_selection::exists) {
              throw std::runtime_error("Relation should be visible, but isn't");
          }

          sel->select_relations({ r_id });

          test_formatter f;
          sel->write_relations(f);
          assert_equal<size_t>(f.m_relations.size(), 1, "number of relations written");

          // we don't want to find out about deviating timestamps here...
          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(r_id, 1, 1, f.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
		  members_t(
		      {
            { element_type_node, n_new_ids[0], "role1" },
	    { element_type_node, n_new_ids[1], "role2" },
	    { element_type_way,  change_tracking->created_way_ids[0].new_id, "" }
		      }
		  ),
		  tags_t({{"boundary", "administrative"}})
              ),
	      f.m_relations[0], "first relation written");
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_relations_with_history({ osm_nwr_id_t( r_id ) }), 1,
	      "number of relations selected");

          test_formatter f2;
          sel->write_relations(f2);
          assert_equal<size_t>(f2.m_relations.size(), 1, "number of relations written");

          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(r_id, 1, 1, f2.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
		  members_t(
		      {
            { element_type_node, n_new_ids[0], "role1" },
	    { element_type_node, n_new_ids[1], "role2" },
	    { element_type_way,  change_tracking->created_way_ids[0].new_id, "" }
		      }
		  ),
		  tags_t({{"boundary", "administrative"}})
              ),
	      f2.m_relations[0], "first relation written");
        }

      }

      // Create two relations with the same old_id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, {}, {});
          rel_updater->add_relation(1, -1, {}, {{"key", "value"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for duplicate old_ids");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP 400 Bad request");
        }
      }

      // Create one relation with self reference
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Relation", -1, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for one relation w/ self reference");

        } catch (http::exception& e) {
    	  if (e.code() != 400)
   	      throw std::runtime_error("One relation w/ self reference: Expected HTTP 400 Bad request");
        }
      }

      // Create two relations with references to each other
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Relation", -2, "role1" }}, {{"key1", "value1"}});
          rel_updater->add_relation(1, -2, { { "Relation", -1, "role2" }}, {{"key2", "value2"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for Relations with reference to each other");

        } catch (http::exception& e) {
    	  if (e.code() != 400)
   	      throw std::runtime_error("Relations with reference to each other: Expected HTTP 400 Bad request");
        }
      }

      // Create two relations with parent/child relationship
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();

        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { }, {{"key1", "value1"}});
          rel_updater->add_relation(1, -2, { { "Relation", -1, "role2" }}, {{"key2", "value2"}});
          rel_updater->process_new_relations();
        } catch (http::exception& e) {
           std::cerr << e.what() << std::endl;
           throw std::runtime_error("Create two relations w/ parent/child: HTTP Exception unexpected");
        }
      
        upd->commit();

        if (change_tracking->created_relation_ids.size() != 2)
  	       throw std::runtime_error("Expected 2 entry in created_relation_ids");

        relation_id_1 = change_tracking->created_relation_ids[0].new_id;
        relation_version_1 = change_tracking->created_relation_ids[0].new_version;

        relation_id_2 = change_tracking->created_relation_ids[1].new_id;
        relation_version_2 = change_tracking->created_relation_ids[1].new_version;

        {
          auto sel = tdb.get_data_selection();
          if (sel->check_relation_visibility(relation_id_1) != data_selection::exists) {
              throw std::runtime_error("Relation should be visible, but isn't");
          }

          if (sel->check_relation_visibility(relation_id_2) != data_selection::exists) {
              throw std::runtime_error("Relation should be visible, but isn't");
          }

          sel->select_relations({relation_id_1, relation_id_2});

          test_formatter f;
          sel->write_relations(f);
          assert_equal<size_t>(f.m_relations.size(), 2, "number of relations written");
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_relations_with_history({relation_id_1, relation_id_2}), 2,
	      "number of relations selected");

          test_formatter f2;
          sel->write_relations(f2);
          assert_equal<size_t>(f2.m_relations.size(), 2, "number of relations written");
        }
      }

      // Create relation with unknown node placeholder id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Node", -10, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for unknown node placeholder id");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP 400 Bad request");
        }
      }

      // Create relation with unknown way placeholder id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Way", -10, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for unknown way placeholder id");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP 400 Bad request");
        }
      }

      // Create relation with unknown relation placeholder id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Relation", -10, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for unknown way placeholder id");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP 400 Bad request");
        }
      }

      // Change existing relation
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

        rel_updater->modify_relation(1, relation_id, relation_version,
	     {
	      { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), "stop_position" },
	      { "Way",  static_cast<osm_nwr_signed_id_t>(way_new_id), "outer" }
	     },
	     {{"admin_level", "4"}, {"boundary","administrative"}}
        );
        rel_updater->process_modify_relations();
        upd->commit();

        if (change_tracking->modified_relation_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in modified_relation_ids");


        if (change_tracking->modified_relation_ids[0].new_version != 2)
  	throw std::runtime_error("Expected new version == 2");

        if (change_tracking->modified_relation_ids[0].new_id != relation_id)
  	throw std::runtime_error((boost::format("Expected new_id == relation_id, %1%, %2%")
                                   % change_tracking->modified_relation_ids[0].new_id
  				 % relation_id).str());

        relation_version = change_tracking->modified_relation_ids[0].new_version;

        {
          // verify current tables
          auto sel = tdb.get_data_selection();
          sel->select_relations({ relation_id });

          test_formatter f;
          sel->write_relations(f);
          assert_equal<size_t>(f.m_relations.size(), 1, "number of relations written");

          // we don't want to find out about deviating timestamps here...
          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(relation_id, relation_version, 1, f.m_relations[0].elem.timestamp, 1, std::string("user_1"), true),
		  members_t(
		      {
            { element_type_node, node_new_ids[0], "stop_position" },
	    { element_type_way,  way_new_id, "outer" }
		      }
		  ),
		  tags_t({{"admin_level", "4"}, {"boundary","administrative"}})
              ),
	      f.m_relations[0], "first relation written");
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_relations_with_history({ osm_nwr_id_t(relation_id) }), 2,
	      "number of relations selected");

          test_formatter f2;
          sel->write_relations(f2);
          assert_equal<size_t>(f2.m_relations.size(), 2, "number of relations written");

          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(relation_id, relation_version, 1, f2.m_relations[1].elem.timestamp, 1, std::string("user_1"), true),
		  members_t(
		      {
            { element_type_node, node_new_ids[0], "stop_position" },
	    { element_type_way,  way_new_id, "outer" }
		      }
		  ),
		  tags_t({{"admin_level", "4"}, {"boundary","administrative"}})
              ),
	      f2.m_relations[1], "first relation written");
        }

      }

      // Change existing relation with incorrect version number
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, 666,
					{ {"Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), ""} }, {});
           rel_updater->process_modify_relations();
  	 throw std::runtime_error("Modifying a relation with wrong version should raise http conflict error");
        } catch (http::exception &e) {
  	  if (e.code() != 409)
  	    throw std::runtime_error("Expected HTTP 409 Conflict");
        }
      }

      // Change existing relation with incorrect version number and non-existing node id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, 666,
					{ {"Node", static_cast<osm_nwr_signed_id_t>(1434253485634), ""} }, {});
           rel_updater->process_modify_relations();
  	 throw std::runtime_error("Modifying a relation with wrong version and non-existing node member id should raise http conflict error");
        } catch (http::exception &e) {
  	  if (e.code() != 409)
  	    throw std::runtime_error("Expected HTTP 409 Conflict");
        }
      }

      // Change existing relation with unknown node id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
            rel_updater->modify_relation(1, relation_id, relation_version,
 					{ {"Node", 1434253485634, ""} }, {});
            rel_updater->process_modify_relations();
           throw std::runtime_error("Modifying a relation with unknown node id should raise http precondition failed error");
        } catch (http::exception &e) {
            if (e.code() != 412)
              throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Change existing relation with unknown way id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, relation_version,
					{ {"Way", 9574853485634, ""} }, {});
           rel_updater->process_modify_relations();
           throw std::runtime_error("Modifying a way with unknown way id should raise http precondition failed error");
        } catch (http::exception &e) {
            if (e.code() != 412)
              throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Change existing relation with unknown relation id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, relation_version,
					{ {"Relation", 9574853485634, ""} }, {});
           rel_updater->process_modify_relations();
           throw std::runtime_error("Modifying a way with unknown relation id should raise http precondition failed error");
        } catch (http::exception &e) {
            if (e.code() != 412)
              throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Change existing relation with unknown node placeholder id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
            rel_updater->modify_relation(1, relation_id, relation_version,
 					{ {"Node", -10, ""} }, {});
            rel_updater->process_modify_relations();

            throw std::runtime_error("Expected exception for unknown way placeholder id");
          } catch (http::exception &e) {
    	  if (e.code() != 400)
    	    throw std::runtime_error("Expected HTTP 400 Bad request");
          }
      }

      // Change existing relation with unknown way placeholder id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, relation_version,
					{ {"Way", -10, ""} }, {});
           rel_updater->process_modify_relations();

           throw std::runtime_error("Expected exception for unknown way placeholder id");
         } catch (http::exception &e) {
   	  if (e.code() != 400)
   	    throw std::runtime_error("Expected HTTP 400 Bad request");
         }
      }

      // Change existing relation with unknown relation placeholder id
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, relation_version,
					{ {"Relation", -10, ""} }, {});
           rel_updater->process_modify_relations();

           throw std::runtime_error("Expected exception for unknown relation placeholder id");
         } catch (http::exception &e) {
   	  if (e.code() != 400)
   	    throw std::runtime_error("Expected HTTP 400 Bad request");
         }
      }

      // TODO: Change existing relation multiple times
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);



      }

      // Preparation for next test case: create a new relation with node_new_ids[2] as only member
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        rel_updater->add_relation(1, -1,
	  {
	      { "Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[2]), "center" }
	  },
	  {{"boundary", "administrative"}});

        rel_updater->process_new_relations();
        upd->commit();
      }

      // Try to delete node which still belongs to relation, if-unused not set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);

        try {
          node_updater->delete_node(1, node_new_ids[2], 1, false);
          node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a node that is still referenced by relation should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Try to delete node which still belongs to relation, if-unused set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);

        try {
          node_updater->delete_node(1, node_new_ids[2], 1, true);
          node_updater->process_delete_nodes();
        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        if (change_tracking->skip_deleted_node_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in skip_deleted_node_ids");

        if (change_tracking->skip_deleted_node_ids[0].new_version != 1)
  	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_node_ids")
                                   % 1).str());

        if (change_tracking->skip_deleted_node_ids[0].new_id != node_new_ids[2])
  	throw std::runtime_error((boost::format("Expected new id == %1% in skip_deleted_node_ids")
                                   % node_new_ids[2]).str());
      }

      // Try to delete way which still belongs to relation, if-unused not set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
            way_updater->delete_way(1, way_new_id, 1, false);
            way_updater->process_delete_ways();
	  throw std::runtime_error("Deleting a way that is still referenced by relation should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Try to delete way which still belongs to relation, if-unused set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          way_updater->delete_way(1, way_new_id, 1, true);
          way_updater->process_delete_ways();
        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        if (change_tracking->skip_deleted_way_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in skip_deleted_way_ids");

        if (change_tracking->skip_deleted_way_ids[0].new_version != 1)
  	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_way_ids")
                                   % 1).str());

        if (change_tracking->skip_deleted_way_ids[0].new_id != way_new_id)
  	throw std::runtime_error((boost::format("Expected new id == %1% in skip_deleted_way_ids")
                                   % way_new_id).str());
      }

      // Try to delete relation which still belongs to relation, if-unused not set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, relation_id_1, relation_version_1, false);
          rel_updater->process_delete_relations();
	  throw std::runtime_error("Deleting a node relation is still referenced by relation should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP 412 precondition failed");
        }
      }

      // Try to delete relation which still belongs to relation, if-unused set
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, relation_id_1, relation_version_1, true);
          rel_updater->process_delete_relations();
        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        if (change_tracking->skip_deleted_relation_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in skip_deleted_relation_ids");

        if (change_tracking->skip_deleted_relation_ids[0].new_version != 1)
  	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_relation_ids")
                                   % 1).str());

        if (change_tracking->skip_deleted_relation_ids[0].new_id != relation_id_1)
  	throw std::runtime_error((boost::format("Expected new id == %1% in skip_deleted_relation_ids")
                                   % relation_id_1).str());
      }


      // Delete existing relation
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        rel_updater->delete_relation(1, relation_id, relation_version++, false);
        rel_updater->process_delete_relations();
        upd->commit();

        if (change_tracking->deleted_relation_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in deleted_relation_ids");

        if (change_tracking->deleted_relation_ids[0] != relation_id) {
  	  throw std::runtime_error("Expected way_id in deleted_relation_ids");
        }

        {
          auto sel = tdb.get_data_selection();
          if (sel->check_relation_visibility(relation_id) != data_selection::deleted) {
              throw std::runtime_error("Relation should be deleted, but isn't");
          }
        }

        {
          // verify historic tables
          auto sel = tdb.get_data_selection();

          assert_equal<int>(
              sel->select_relations_with_history({ osm_nwr_id_t(relation_id) }), relation_version,
	      "number of relations selected");

          test_formatter f2;
          sel->write_relations(f2);
          assert_equal<size_t>(f2.m_relations.size(), relation_version, "number of relations written");

          assert_equal<test_formatter::relation_t>(
              test_formatter::relation_t(
        	  element_info(relation_id, relation_version, 1, f2.m_relations[relation_version - 1].elem.timestamp, 1, std::string("user_1"), false),
		  members_t(),
		  tags_t()
              ),
	      f2.m_relations[relation_version - 1], "relation deleted");
        }
      }

      // Delete two relation with references to each other
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        rel_updater->delete_relation(1, relation_id_1, relation_version_1, false);
        rel_updater->delete_relation(1, relation_id_2, relation_version_2, false);
        rel_updater->process_delete_relations();
        upd->commit();

        if (change_tracking->deleted_relation_ids.size() != 2)
  	throw std::runtime_error("Expected 2 entries in deleted_relation_ids");

        if (sel->check_relation_visibility(relation_id_1) != data_selection::deleted) {
  	  throw std::runtime_error("Relation should be deleted, but isn't");
        }
        if (sel->check_relation_visibility(relation_id_2) != data_selection::deleted) {
  	  throw std::runtime_error("Relation should be deleted, but isn't");
        }
      }


      // Try to delete already deleted relation (if-unused not set)
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, relation_id, relation_version, false);
          rel_updater->process_delete_relations();
  	  throw std::runtime_error("Deleting a non-existing relation should raise an http gone error");
        } catch (http::exception &e) {
  	  if (e.code() != 410)
  	    throw std::runtime_error("Expected HTTP 410 Gone");
        }
      }

      // Try to delete already deleted relation (if-unused set)
      {
	auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, relation_id, relation_version, true);
          rel_updater->process_delete_relations();
        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        if (change_tracking->skip_deleted_relation_ids.size() != 1)
  	throw std::runtime_error("Expected 1 entry in skip_deleted_relation_ids");

        if (change_tracking->skip_deleted_relation_ids[0].new_version != relation_version)
  	throw std::runtime_error((boost::format("Expected new version == %1% in skip_deleted_relation_ids")
                                   % relation_version).str());
      }

      // Delete non-existing relation
      {
        auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, 424471234567890, 1, false);
          rel_updater->process_delete_relations();
  	  throw std::runtime_error("Deleting a non-existing relation should raise an http not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP 404 Not found");
        }
      }

      // Modify non-existing relation
      {
        auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, 424471234567890, 1, {}, {});
           rel_updater->process_modify_relations();
  	 throw std::runtime_error("Modifying a non-existing relation should trigger a not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP 404 Not found");
        }
      }
    }

  void test_changeset_update(test_database &tdb) {

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

      // Trying to add CHANGESET_MAX_ELEMENTS to empty changeset - should succeed
      try {
	auto upd = tdb.get_data_update();
	auto changeset_updater = upd->get_changeset_updater(1, 1);
	changeset_updater->update_changeset(CHANGESET_MAX_ELEMENTS, {});  // use undefined bbox
      } catch (http::exception & e) {
	  throw std::runtime_error("test_changeset_update:001 - HTTP Exception unexpected");
      }

      // Trying to add CHANGESET_MAX_ELEMENTS + 1 to empty changeset - should fail
      try {
	auto upd = tdb.get_data_update();
	auto changeset_updater = upd->get_changeset_updater(1, 1);
	changeset_updater->update_changeset(CHANGESET_MAX_ELEMENTS + 1, {});
      } catch (http::exception &e) {
	  if (e.code() != 409)
	    throw std::runtime_error("test_changeset_update:002 - Expected HTTP 409 Conflict");
      }
  }


  std::vector<api06::diffresult_t> process_payload(test_database &tdb, osm_changeset_id_t changeset, osm_user_id_t uid, std::string payload)
  {
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();

    auto change_tracking = std::make_shared<api06::OSMChange_Tracking>();

    auto changeset_updater = upd->get_changeset_updater(changeset, uid);
    auto node_updater = upd->get_node_updater(change_tracking);
    auto way_updater = upd->get_way_updater(change_tracking);
    auto relation_updater = upd->get_relation_updater(change_tracking);

    changeset_updater->lock_current_changeset();

    api06::OSMChange_Handler handler(std::move(node_updater), std::move(way_updater),
                                     std::move(relation_updater), changeset);

    api06::OSMChangeXMLParser parser(&handler);

    parser.process_message(payload);

    auto diffresult = change_tracking->assemble_diffresult();

    changeset_updater->update_changeset(handler.get_num_changes(),
                                        handler.get_bbox());

    upd->commit();

    return diffresult;
  }

  void test_osmchange_message(test_database &tdb) {

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

      try {

      // Test unknown changeset id
	auto diffresult = process_payload(tdb, 1234, 1, R"(<?xml version="1.0" encoding="UTF-8"?>
	  <osmChange version="0.6" generator="iD">
	     <create>
		<node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1234">
		   <tag k="highway" v="bus_stop" />
		</node>
	     </create>
	  </osmChange>
	)");
    	 throw std::runtime_error("Test unknown changeset id should trigger a not_found error");
      } catch (http::exception& e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Test unknown changeset id: Expected HTTP 404 Not found");
      }

      // Test more complex examples, including XML parsing

      // Forward relation member declarations

      // Example from https://github.com/openstreetmap/iD/issues/3208#issuecomment-281942743
   
      try {

  // Relation id -3 has a relation member with forward reference to relation id -4

	  auto diffresult = process_payload(tdb, 1, 1, R"(<?xml version="1.0" encoding="UTF-8"?>
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
  
	)");
    	 throw std::runtime_error("Forward relation definition should trigger a bad request error");
      } catch (http::exception& e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Forward relation definition: Expected HTTP 400 Bad request");
      }

  // Testing correct parent/child sequence
  try {

      auto diffresult = process_payload(tdb, 1, 1, R"(<?xml version="1.0" encoding="UTF-8"?>
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
  
	)");

      assert_equal<int>(diffresult.size(), 5, "diffresult rows written");

      std::vector<osm_nwr_signed_id_t> old_ids{ -5, -6, -2, -3, -4};
      std::vector<object_type> obj_type{ object_type::node,
	                                 object_type::node,
					 object_type::relation,
					 object_type::relation,
					 object_type::relation };

      for (int i = 0; i < 5; i++) {
	  assert_equal<osm_nwr_signed_id_t>(old_ids[i], diffresult[i].old_id, "diffresult old_id");
	  assert_equal<osm_version_t>(1, diffresult[i].new_version, "diffresult new_version");
	  assert_equal<int>(static_cast<int>(obj_type[i]), static_cast<int>(diffresult[i].obj_type), "diffresult obj_type");
	  assert_equal<int>(static_cast<int>(operation::op_create), static_cast<int>(diffresult[i].op), "diffresult operation");
	  assert_equal<bool>(false, diffresult[i].deletion_skipped, "diffresult deletion_skipped");
      }

      } catch (http::exception& e) {
        throw std::runtime_error("Correct forward relation member reference should not trigger an exception");
      }
  }

  void test_osmchange_end_to_end(test_database &tdb) {

    // Prepare users, changesets

    tdb.run_sql(R"(
	 INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
	 VALUES
	   (1, 'demo@example.com', '3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=',
                                     'sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=',
                                     '2013-11-14T02:10:00Z', 'demo', true, 'confirmed'),
	   (2, 'user_2@example.com', '', '', '2013-11-14T02:10:00Z', 'user_2', false, 'active');

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

        )"
    );

    const std::string baseauth = "Basic ZGVtbzpwYXNzd29yZA==";
    const std::string generator = "Test";

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    null_rate_limiter limiter;
    routes route;

    // User providing wrong password
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", "Basic ZGVtbzppbnZhbGlkcGFzc3dvcmQK");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="2"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 401)
	  throw std::runtime_error("Expected HTTP 401 Unauthorized: wrong user/password");
    }


    // User logging on with display name (different case)
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", "Basic REVNTzpwYXNzd29yZA==");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-1" lon="11" lat="46" changeset="1"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 200)
	  throw std::runtime_error("Expected HTTP 200 OK: Log on with display name, different case");
    }

    // User logging on with email address rather than display name
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", "Basic ZGVtb0BleGFtcGxlLmNvbTpwYXNzd29yZA==");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-1" lon="11" lat="46" changeset="1"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 200)
	  throw std::runtime_error("Expected HTTP 200 OK: Log on with email address");
    }


    // User logging on with email address with different case and additional whitespace rather than display name
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", "Basic ICAgZGVtb0BleGFtcGxlLkNPTSAgIDpwYXNzd29yZA==");
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-1" lon="11" lat="46" changeset="1"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 200)
	  throw std::runtime_error("Expected HTTP 200 OK: Log on with email address, whitespace, different case");
    }



    // User is blocked (needs_view)
    {
        tdb.run_sql(R"(UPDATE user_blocks SET needs_view = true where user_id = 1;)");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 403)
	  throw std::runtime_error("Expected HTTP 403 Forbidden: user blocked (needs view)");

	tdb.run_sql(R"(UPDATE user_blocks SET needs_view = false where user_id = 1;)");
    }

    // User is blocked for 1 hour
    {
        tdb.run_sql(R"(UPDATE user_blocks
                         SET needs_view = false,
                             ends_at = now() at time zone 'utc' + ('1 hour' ::interval)
                         WHERE user_id = 1;)");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="1"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));


	if (req.response_status() != 403)
	  throw std::runtime_error("Expected HTTP 403 Forbidden: user blocked for 1 hour");

	tdb.run_sql(R"(UPDATE user_blocks
                          SET needs_view = false,
                              ends_at = now() at time zone 'utc' - ('1 hour' ::interval)
                          WHERE user_id = 1;)");
    }

    // Try to post a changeset, where the URL points to a different URL than the payload
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="2"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 409)
	  throw std::runtime_error("Expected HTTP 409 Conflict: Payload and URL changeset id differ");
    }

    // Try to post a changeset, where the user doesn't own the changeset
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/4/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
	     <osmChange version="0.6" generator="iD">
	     <create><node id="-5" lon="11.625506992810122" lat="46.866699181636555" version="0" changeset="4"/></create>
             </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 409)
	  throw std::runtime_error("Expected HTTP 409 Conflict: User doesn't own the changeset");
    }

    // Try to add a node to a changeset that already has 10000 elements (=max)
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/2/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
		  <osmChange version="0.6" generator="iD">
		     <create><node id="-5" lon="11" lat="46" version="0" changeset="2"/></create>
		  </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 409)
	  std::runtime_error("Expected HTTP 409 Conflict: Cannot add more elements to changeset");
    }

    // Try to add a node to a changeset that is already closed
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/3/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

	req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
		  <osmChange version="0.6" generator="iD">
		     <create><node id="-5" lon="11" lat="46" version="0" changeset="3"/></create>
		  </osmChange>)" );

	// execute the request
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	if (req.response_status() != 409)
	  std::runtime_error("Expected HTTP 409 Conflict: Changeset already closed");
    }

    // Try to add a nodes, ways, relations to a changeset
    {

        // Set sequences to new start values

        tdb.run_sql(R"(  SELECT setval('current_nodes_id_seq', 12000000000, false);
                         SELECT setval('current_ways_id_seq', 14000000000, false);
                         SELECT setval('current_relations_id_seq', 18000000000, false);
                     )");

	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

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
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	// std::cout << "Response was:\n----------------------\n" << req.buffer().str() << "\n";

	if (req.response_status() != 200)
	  std::runtime_error("Expected HTTP 200 OK: Create new node");
    }

    // Try to add, modify and delete nodes, ways, relations in changeset
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

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
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	// std::cout << "Response was:\n----------------------\n" << req.buffer().str() << "\n";

	if (req.response_status() != 200)
	  std::runtime_error("Expected HTTP 200 OK: add, modify and delete nodes, ways, relations in changeset");
    }

    // Multiple operations on the same node id -1
    {
	// set up request headers from test case
	test_request req;
	req.set_header("REQUEST_METHOD", "POST");
	req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
	req.set_header("HTTP_AUTHORIZATION", baseauth);
	req.set_header("REMOTE_ADDR", "127.0.0.1");

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
	process_request(req, limiter, generator, route, sel_factory, upd_factory, std::shared_ptr<oauth::store>(nullptr));

	// std::cout << "Response was:\n----------------------\n" << req.buffer().str() << "\n";

	if (req.response_status() != 200)
	  std::runtime_error("Expected HTTP 200 OK: Multiple operations on the same node id -1");

  }

}

} // anonymous namespace

int main(int, char **) {

  try {
      test_database tdb;
      tdb.setup();

      tdb.run_update(std::function<void(test_database&)>(&test_single_nodes));

      tdb.run_update(std::function<void(test_database&)>(&test_single_ways));

      tdb.run_update(std::function<void(test_database&)>(&test_single_relations));

      tdb.run_update(std::function<void(test_database&)>(&test_changeset_update));

      tdb.run_update(std::function<void(test_database&)>(&test_osmchange_message));

      tdb.run_update(std::function<void(test_database&)>(&test_osmchange_end_to_end));


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
