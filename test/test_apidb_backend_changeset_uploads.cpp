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

    auto change_tracking = std::make_shared<OSMChange_Tracking>();


    // Create new node
    {

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


    // Change existing node
    {
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

         node_updater->modify_node(lat, lon, 1, node_id, node_version++, {{"key", "value" + i}});
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
      auto sel = tdb.get_data_selection();
      auto upd = tdb.get_data_update();
      auto node_updater = upd->get_node_updater(change_tracking);

      try {
	  node_updater->delete_node(1, node_id, node_version, false);
	  node_updater->process_delete_nodes();
	  throw std::runtime_error("Deleting a non-existing node should raise an http gone error");
      } catch (http::exception &e) {
	  if (e.code() != 410)
	    throw std::runtime_error("Expected HTTP/410 Gone");
      }

    }

    // Try to delete already deleted node (if-unused set)
    {
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
  }

} // anonymous namespace

int main(int, char **) {

  try {
      test_database tdb;
      tdb.setup();

      tdb.run_update(boost::function<void(test_database&)>(&test_single_nodes));

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
