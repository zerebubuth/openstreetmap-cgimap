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
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
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


    // Create two nodes with the same old_id
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
	    throw std::runtime_error("Expected HTTP/400 Bad request");
      }
    }

    // Change existing node
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
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

    // Change existing node with incorrect version number
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
         node_updater->modify_node(40, 50, 1, node_id, 666, {});
         node_updater->process_modify_nodes();
	 throw std::runtime_error("Modifying a node with wrong version should raise http conflict error");
      } catch (http::exception &e) {
	  if (e.code() != 409)
	    throw std::runtime_error("Expected HTTP/409 Conflict");
      }
    }

    // Change existing node multiple times
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
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
    }

    // Delete existing node
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
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

      if (sel->check_node_visibility(node_id) != data_selection::deleted) {
	  throw std::runtime_error("Node should be deleted, but isn't");
      }
    }

    // Try to delete already deleted node (if-unused not set)
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
	  node_updater->delete_node(1, node_id, node_version, false);
	  node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a deleted node should raise an http gone error");
      } catch (http::exception &e) {
	  if (e.code() != 410)
	    throw std::runtime_error("Expected HTTP/410 Gone");
      }
    }

    // Try to delete already deleted node (if-unused set)
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
	  node_updater->delete_node(1, 424471234567890, 1, false);
	  node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a non-existing node should raise an http not found error");
      } catch (http::exception &e) {
	  if (e.code() != 404)
	    throw std::runtime_error("Expected HTTP/404 Not found");
      }
    }

    // Modify non-existing node
    {
      auto change_tracking = std::make_shared<OSMChange_Tracking>();
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
         node_updater->modify_node(40, 50, 1, 4712334567890, 1, {});
         node_updater->process_modify_nodes();
	 throw std::runtime_error("Modifying a non-existing node should trigger a not found error");
      } catch (http::exception &e) {
	  if (e.code() != 404)
	    throw std::runtime_error("Expected HTTP/404 Not found");
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
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);

        node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
        node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
        node_updater->process_new_nodes();

        way_updater->add_way(1, -1, {{ -1}, { -2}}, { {"highway", "path"}});
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


      // Create two ways with the same old_id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          node_updater->add_node(0, 0 , 1, -1, {});
          node_updater->add_node(10, 20 , 1, -2, {});
          node_updater->process_new_nodes();

          way_updater->add_way(1, -1, {{ -1}, { -2}}, { {"highway", "path"}});
          way_updater->add_way(1, -1, {{ -2}, { -1}}, { {"highway", "path"}});
          way_updater->process_new_ways();

          throw std::runtime_error("Expected exception for duplicate old_ids");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP/400 Bad request");
        }
      }

      // Create way with unknown placeholder ids
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          way_updater->add_way(1, -1, {{ -1}, { -2}}, { {"highway", "path"}});
          way_updater->process_new_ways();

          throw std::runtime_error("Expected exception for unknown placeholder ids");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP/400 Bad request");
        }
      }

      // Change existing way
     {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
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
  	  f.m_ways[0], "first way written");
      }

      // Change existing way with incorrect version number
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, 666, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0])}, {});
           way_updater->process_modify_ways();
  	 throw std::runtime_error("Modifying a way with wrong version should raise http conflict error");
        } catch (http::exception &e) {
  	  if (e.code() != 409)
  	    throw std::runtime_error("Expected HTTP/409 Conflict");
        }
      }

      // Change existing way with unknown node id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, way_version, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), 9574853485634}, {});
           way_updater->process_modify_ways();
           throw std::runtime_error("Modifying a way with unknown node id should raise http precondition failed error");
        } catch (http::exception &e) {
            if (e.code() != 412)
              throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Change existing way with unknown placeholder node id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, way_id, way_version, {-5}, {});
           way_updater->process_modify_ways();
           throw std::runtime_error("Modifying a way with unknown placeholder node id should raise http bad request");
        } catch (http::exception &e) {
            if (e.code() != 400)
              throw std::runtime_error("Expected HTTP/400 bad request");
        }
      }

      // TODO: Change existing way multiple times
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);




      }

      // Try to delete node which still belongs to way, if-unused not set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);

        try {
          node_updater->delete_node(1, node_new_ids[0], 1, false);
          node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a node that is still referenced by way should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Try to delete node which still belongs to way, if-unused set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
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

        if (sel->check_way_visibility(way_id) != data_selection::deleted) {
  	  throw std::runtime_error("Way should be deleted, but isn't");
        }
      }

      // Try to delete already deleted node (if-unused not set)
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
  	  way_updater->delete_way(1, way_id, way_version, false);
  	  way_updater->process_delete_ways();
  	  throw std::runtime_error("Deleting a non-existing way should raise an http gone error");
        } catch (http::exception &e) {
  	  if (e.code() != 410)
  	    throw std::runtime_error("Expected HTTP/410 Gone");
        }
      }

      // Try to delete already deleted node (if-unused set)
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
        auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
          way_updater->delete_way(1, 424471234567890, 1, false);
          way_updater->process_delete_ways();
  	  throw std::runtime_error("Deleting a non-existing way should raise an http not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP/404 Not found");
        }
      }

      // Modify non-existing way
      {
        auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
           way_updater->modify_way(1, 424471234567890, 1, {static_cast<osm_nwr_signed_id_t>(node_new_ids[0])}, {});
           way_updater->process_modify_ways();
  	 throw std::runtime_error("Modifying a non-existing way should trigger a not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP/404 Not found");
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
      osm_nwr_id_t node_new_ids[2];
      osm_nwr_id_t way_new_id;

      osm_nwr_id_t relation_id_1;
      osm_version_t relation_version_1;
      osm_nwr_id_t relation_id_2;
      osm_version_t relation_version_2;

      // Create new relation with two nodes, and one way
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

        node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"}, {"ele", "863"} });
        node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
        node_updater->process_new_nodes();

        way_updater->add_way(1, -1, {{ -1}, { -2}}, { {"highway", "path"}});
        way_updater->process_new_ways();

        for (const auto id : change_tracking->created_node_ids) {
           node_new_ids[-1 * id.old_id - 1] = id.new_id;
        }

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


      // Create new relation with two nodes, and one way, only placeholder ids
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);

        node_updater->add_node(-25.3448570, 131.0325171, 1, -1, { {"name", "Uluṟu"} });
        node_updater->add_node(-25.3448570, 131.2325171, 1, -2, { });
        node_updater->process_new_nodes();

        way_updater->add_way(1, -1, {{ -1}, { -2}}, { {"highway", "track"}});
        way_updater->process_new_ways();

        rel_updater->add_relation(1, -1,
	  {
	      { "Node", -1, "role1" },
	      { "Node", -2, "role2" },
	      { "Way",  -1, "" }
	  },
	  {{"boundary", ""}});

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
  	      tags_t({{"boundary", ""}})
  	  ),
  	  f.m_relations[0], "first relation written");
      }

      // Create two relations with the same old_id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
  	    throw std::runtime_error("Expected HTTP/400 Bad request");
        }
      }

      // Create two relations with references to each other
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Relation", -2, "role1" }}, {{"key1", "value1"}});
          rel_updater->add_relation(1, -2, { { "Relation", -1, "role2" }}, {{"key2", "value2"}});
          rel_updater->process_new_relations();

        } catch (http::exception& e) {
  	  throw std::runtime_error("HTTP Exception unexpected");
        }

        upd->commit();

        if (change_tracking->created_relation_ids.size() != 2)
  	throw std::runtime_error("Expected 2 entry in created_relation_ids");

        relation_id_1 = change_tracking->created_relation_ids[0].new_id;
        relation_version_1 = change_tracking->created_relation_ids[0].new_version;

        relation_id_2 = change_tracking->created_relation_ids[1].new_id;
        relation_version_2 = change_tracking->created_relation_ids[1].new_version;

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

      // Create relation with unknown node placeholder id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Node", -10, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for unknown node placeholder id");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP/400 Bad request");
        }
      }

      // Create relation with unknown way placeholder id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Way", -10, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for unknown way placeholder id");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP/400 Bad request");
        }
      }

      // Create relation with unknown relation placeholder id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->add_relation(1, -1, { { "Relation", -10, "role1" }}, {{"key1", "value1"}});
          rel_updater->process_new_relations();

          throw std::runtime_error("Expected exception for unknown way placeholder id");
        } catch (http::exception &e) {
  	  if (e.code() != 400)
  	    throw std::runtime_error("Expected HTTP/400 Bad request");
        }
      }

      // Change existing relation
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
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

      // Change existing relation with incorrect version number
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, relation_id, 666,
					{ {"Node", static_cast<osm_nwr_signed_id_t>(node_new_ids[0]), ""} }, {});
           rel_updater->process_modify_relations();
  	 throw std::runtime_error("Modifying a way with wrong version should raise http conflict error");
        } catch (http::exception &e) {
  	  if (e.code() != 409)
  	    throw std::runtime_error("Expected HTTP/409 Conflict");
        }
      }

      // Change existing relation with unknown node id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
              throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Change existing relation with unknown way id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
              throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Change existing relation with unknown relation id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
              throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Change existing relation with unknown node placeholder id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
    	    throw std::runtime_error("Expected HTTP/400 Bad request");
          }
      }

      // Change existing relation with unknown way placeholder id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
   	    throw std::runtime_error("Expected HTTP/400 Bad request");
         }
      }

      // Change existing relation with unknown relation placeholder id
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
   	    throw std::runtime_error("Expected HTTP/400 Bad request");
         }
      }

      // TODO: Change existing relation multiple times
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);
        auto rel_updater = upd->get_relation_updater(change_tracking);



      }

      // Try to delete node which still belongs to relation, if-unused not set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto node_updater = upd->get_node_updater(change_tracking);

        try {
          node_updater->delete_node(1, node_new_ids[0], 1, false);
          node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a node that is still referenced by relation should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Try to delete node which still belongs to relation, if-unused set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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

      // Try to delete way which still belongs to relation, if-unused not set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto way_updater = upd->get_way_updater(change_tracking);

        try {
            way_updater->delete_way(1, way_new_id, 1, false);
            way_updater->process_delete_ways();
	  throw std::runtime_error("Deleting a way that is still referenced by relation should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Try to delete way which still belongs to relation, if-unused set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
      }

      // Try to delete relation which still belongs to relation, if-unused not set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, relation_id_1, relation_version_1, false);
          rel_updater->process_delete_relations();
	  throw std::runtime_error("Deleting a node relation is still referenced by relation should raise an exception");
        } catch (http::exception &e) {
  	  if (e.code() != 412)
  	    throw std::runtime_error("Expected HTTP/412 precondition failed");
        }
      }

      // Try to delete relation which still belongs to relation, if-unused set
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
      }


      // Delete existing relation
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
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

        if (sel->check_relation_visibility(relation_id) != data_selection::deleted) {
  	  throw std::runtime_error("Relation should be deleted, but isn't");
        }
      }

      // Delete two relation with references to each other
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, relation_id, relation_version, false);
          rel_updater->process_delete_relations();
  	  throw std::runtime_error("Deleting a non-existing relation should raise an http gone error");
        } catch (http::exception &e) {
  	  if (e.code() != 410)
  	    throw std::runtime_error("Expected HTTP/410 Gone");
        }
      }

      // Try to delete already deleted relation (if-unused set)
      {
	auto change_tracking = std::make_shared<OSMChange_Tracking>();
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
        auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
          rel_updater->delete_relation(1, 424471234567890, 1, false);
          rel_updater->process_delete_relations();
  	  throw std::runtime_error("Deleting a non-existing relation should raise an http not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP/404 Not found");
        }
      }

      // Modify non-existing relation
      {
        auto change_tracking = std::make_shared<OSMChange_Tracking>();
        auto sel = tdb.get_data_selection();
        auto upd = tdb.get_data_update();
        auto rel_updater = upd->get_relation_updater(change_tracking);

        try {
           rel_updater->modify_relation(1, 424471234567890, 1, {}, {});
           rel_updater->process_modify_relations();
  	 throw std::runtime_error("Modifying a non-existing relation should trigger a not found error");
        } catch (http::exception &e) {
  	  if (e.code() != 404)
  	    throw std::runtime_error("Expected HTTP/404 Not found");
        }
      }
    }

  void process_payload(test_database &tdb, std::string payload)
  {
    auto sel = tdb.get_data_selection();
    auto upd = tdb.get_data_update();

    auto changeset = 1;
    auto uid = 1;

    auto change_tracking = std::make_shared<OSMChange_Tracking>();

    auto changeset_updater = upd->get_changeset_updater(changeset, uid);
    auto node_updater = upd->get_node_updater(change_tracking);
    auto way_updater = upd->get_way_updater(change_tracking);
    auto relation_updater = upd->get_relation_updater(change_tracking);

    changeset_updater->lock_current_changeset();

    OSMChange_Handler handler(std::move(node_updater), std::move(way_updater),
                              std::move(relation_updater), changeset, uid);

    OSMChangeXMLParser parser(&handler);

    parser.process_message(payload);

    change_tracking->populate_orig_sequence_mapping();

    changeset_updater->update_changeset(handler.get_num_changes(),
                                        handler.get_bbox());

    upd->commit();
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

      // Test more complex examples, including XML parsing

      try {

	// Example in https://github.com/openstreetmap/iD/issues/3208#issuecomment-281942743

	process_payload(tdb, R"(<?xml version="1.0" encoding="UTF-8"?>
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
  
	)" );
      } catch (http::exception& e) {
	  std::cerr << e.what() << std::endl;
	  throw std::runtime_error("HTTP Exception unexpected");
      }

  }

} // anonymous namespace

int main(int, char **) {

  try {
      test_database tdb;
      tdb.setup();

      tdb.run_update(boost::function<void(test_database&)>(&test_single_nodes));

      tdb.run_update(boost::function<void(test_database&)>(&test_single_ways));

      tdb.run_update(boost::function<void(test_database&)>(&test_single_relations));

      tdb.run_update(boost::function<void(test_database&)>(&test_osmchange_message));

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
