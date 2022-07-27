#ifndef DATA_SELECTION_HPP
#define DATA_SELECTION_HPP

#include "cgimap/types.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include <chrono>
#include <memory>
#include <sstream>
#include <vector>

/**
 * represents a selected set of data which can be written out to
 * an output_formatter and manipulated by a nice set of commands
 * suited for OSM relational data manipulations.
 */
class data_selection {
public:
  enum visibility_t { exists, deleted, non_exist };

  virtual ~data_selection() = default;

  data_selection() = default;

  data_selection(const data_selection&) = delete;
  data_selection& operator=(const data_selection&) = delete;

  data_selection(data_selection&&) = delete;
  data_selection& operator=(data_selection&&) = delete;

  /******************* output functions ************************/

  /// write the nodes to an output formatter
  virtual void write_nodes(output_formatter &formatter) = 0;

  /// write the ways to an output formatter
  virtual void write_ways(output_formatter &formatter) = 0;

  /// write the relations to an output formatter
  virtual void write_relations(output_formatter &formatter) = 0;

  /// does this data selection support changesets?
  virtual void write_changesets(output_formatter &formatter,
                                const std::chrono::system_clock::time_point &now) = 0;

  /******************* information functions *******************/

  // check if the node is visible, deleted or has never existed
  virtual visibility_t check_node_visibility(osm_nwr_id_t id) = 0;

  // check if the way is visible, deleted or has never existed
  virtual visibility_t check_way_visibility(osm_nwr_id_t id) = 0;

  // check if the relation is visible, deleted or has never existed
  virtual visibility_t check_relation_visibility(osm_nwr_id_t id) = 0;

  /******************* manipulation functions ******************/

  /// select the nodes in the vector, returning the number of nodes
  /// which are selected now which weren't selected before.
  virtual int select_nodes(const std::vector<osm_nwr_id_t> &) = 0;

  /// select the ways in the vector, returning the number of ways
  /// which are selected now which weren't selected before.
  virtual int select_ways(const std::vector<osm_nwr_id_t> &) = 0;

  /// select the relations in the vector, returning the number of
  /// relations which are selected now which weren't selected before.
  virtual int select_relations(const std::vector<osm_nwr_id_t> &) = 0;

  /// given a bounding box, select nodes within that bbox up to a limit of
  /// max_nodes
  virtual int select_nodes_from_bbox(const bbox &bounds, int max_nodes) = 0;

  /// selects the node members of any already selected relations
  virtual void select_nodes_from_relations() = 0;

  /// selects all ways that contain selected nodes
  virtual void select_ways_from_nodes() = 0;

  /// selects all ways that are members of selected relations
  virtual void select_ways_from_relations() = 0;

  /// select all relations that contain selected ways
  virtual void select_relations_from_ways() = 0;

  /// select nodes which are used in selected ways
  virtual void select_nodes_from_way_nodes() = 0;

  /// select relations which include selected nodes
  virtual void select_relations_from_nodes() = 0;

  /// select relations which include selected relations
  virtual void select_relations_from_relations(bool drop_relations = false) = 0;

  /// select relations which are members of selected relations
  virtual void select_relations_members_of_relations() = 0;

  /// drop any nodes which are in the current selection
  virtual void drop_nodes() = 0;

  /// drop any ways which are in the current selection
  virtual void drop_ways() = 0;

  /// drop any relations which are in the current selection
  virtual void drop_relations() = 0;

  /******************* historical functions ********************/

  /// select the given (id, version) versions of nodes, returning the number of
  /// nodes added to the selected set.
  virtual int select_historical_nodes(const std::vector<osm_edition_t> &) = 0;

  /// select all versions of the node with the given IDs. returns the number of
  /// distinct (id, version) pairs selected.
  virtual int select_nodes_with_history(const std::vector<osm_nwr_id_t> &) = 0;

  /// select the given (id, version) versions of ways, returning the number of
  /// ways added to the selected set.
  virtual int select_historical_ways(const std::vector<osm_edition_t> &) = 0;

  /// select all versions of the way with the given IDs. returns the number of
  /// distinct (id, version) pairs selected.
  virtual int select_ways_with_history(const std::vector<osm_nwr_id_t> &) = 0;

  /// select the given (id, version) versions of relations, returning the number
  /// of relations added to the selected set.
  virtual int select_historical_relations(const std::vector<osm_edition_t> &) = 0;

  /// select all versions of the relation with the given IDs. returns the number
  /// of distinct (id, version) pairs selected.
  virtual int select_relations_with_history(const std::vector<osm_nwr_id_t> &) = 0;

  /// if true, then include redactions in returned data. should default to
  /// false.
  virtual void set_redactions_visible(bool visible) = 0;

  /// select all versions of nodes, ways and relations which were added as part
  /// of any of the changesets with the given IDs. returns the number of
  /// distinct (element_type, id, version) tuples selected.
  virtual int select_historical_by_changesets(
    const std::vector<osm_changeset_id_t> &) = 0;

  /****************** changeset functions **********************/

  /// select specified changesets, returning the number of
  /// changesets selected.
  virtual int select_changesets(const std::vector<osm_changeset_id_t> &) = 0;

  /// select the changeset discussions as well. this effectively
  /// just sets a flag - by default, discussions are not included,
  /// if this is called then discussions will be included.
  virtual void select_changeset_discussions() = 0;

  /****************** user functions **********************/

  // does this data selection support user details?
  virtual bool supports_user_details() const = 0;

  // is user currently blocked?
  virtual bool is_user_blocked(const osm_user_id_t) = 0;

  virtual bool get_user_id_pass(const std::string& display_name, osm_user_id_t &,
				std::string & pass_crypt, std::string & pass_salt) = 0;

  /**
   * factory for the creation of data selections. this abstracts away
   * the creation process of transactions, and allows some up-front
   * work to be done. for example, setting up prepared statements on
   * a database connection.
   */
  struct factory {
    virtual ~factory() = default;

    factory() = default;

    factory(const factory&) = delete;
    factory& operator=(const factory&) = delete;

    factory(factory&&) = delete;
    factory& operator=(factory&&) = delete;

    /// get a handle to a selection which can be used to build up
    /// a working set of data.
    virtual std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) = 0;

    virtual std::unique_ptr<Transaction_Owner_Base> get_default_transaction() = 0;
  };
};


// parses psql array based on specs given
// https://www.postgresql.org/docs/current/static/arrays.html#ARRAYS-IO
std::vector<std::string> psql_array_to_vector(std::string str);

#endif /* DATA_SELECTION_HPP */
