#ifndef READONLY_PGSQL_SELECTION_HPP
#define READONLY_PGSQL_SELECTION_HPP

#include "cgimap/data_selection.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include <pqxx/pqxx>
#include <boost/program_options.hpp>
#include <chrono>
#include <memory>
#include <set>

/**
 * a selection which operates against a readonly (e.g: replicating
 * slave) PostgreSQL database. This is needed because, currently,
 * slaves can't create XIDs which would be necessary to create
 * temporary tables. Instead, the state of the selection must be
 * stored in-memory in cgimap's memory.
 */
class readonly_pgsql_selection : public data_selection {


public:
  readonly_pgsql_selection(Transaction_Owner_Base& to);
  ~readonly_pgsql_selection();

  void write_nodes(output_formatter &formatter) override;
  void write_ways(output_formatter &formatter) override;
  void write_relations(output_formatter &formatter) override;
  void write_changesets(output_formatter &formatter, const std::chrono::system_clock::time_point &now) override;

  visibility_t check_node_visibility(osm_nwr_id_t id) override;
  visibility_t check_way_visibility(osm_nwr_id_t id) override;
  visibility_t check_relation_visibility(osm_nwr_id_t id) override;

  int select_nodes(const std::vector<osm_nwr_id_t> &)override ;
  int select_ways(const std::vector<osm_nwr_id_t> &) override;
  int select_relations(const std::vector<osm_nwr_id_t> &) override;
  int select_nodes_from_bbox(const bbox &bounds, int max_nodes) override;
  void select_nodes_from_relations() override;
  void select_ways_from_nodes() override;
  void select_ways_from_relations() override;
  void select_relations_from_ways() override;
  void select_nodes_from_way_nodes() override;
  void select_relations_from_nodes() override;
  void select_relations_from_relations(bool drop_relations = false) override;
  void select_relations_members_of_relations() override;

  int select_changesets(const std::vector<osm_changeset_id_t> &) override;
  void select_changeset_discussions() override;

  void drop_nodes() override;
  void drop_ways() override;
  void drop_relations() override;

  int select_historical_nodes(const std::vector<osm_edition_t> &) override;
  int select_historical_ways(const std::vector<osm_edition_t> &) override;
  int select_historical_relations(const std::vector<osm_edition_t> &) override;
  int select_nodes_with_history(const std::vector<osm_nwr_id_t> &) override;
  int select_ways_with_history(const std::vector<osm_nwr_id_t> &) override;
  int select_relations_with_history(const std::vector<osm_nwr_id_t> &) override;
  void set_redactions_visible(bool) override;
  int select_historical_by_changesets(const std::vector<osm_changeset_id_t> &) override;

  bool supports_user_details() const override;
  bool is_user_blocked(const osm_user_id_t) override;
  bool get_user_id_pass(const std::string&, osm_user_id_t &, std::string &, std::string &) override;



  /**
   * a factory for the creation of read-only selections
   */
  class factory : public data_selection::factory {
  public:
    factory(const boost::program_options::variables_map &);
    virtual ~factory();
    std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) override;
    std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override;

  private:
    pqxx::connection m_connection;
    pqxx::quiet_errorhandler m_errorhandler;
  };

private:
  std::set< osm_changeset_id_t > extract_changeset_ids(pqxx::result& result);
  void fetch_changesets(const std::set< osm_changeset_id_t >& ids, std::map<osm_changeset_id_t, changeset> & cc);

  Transaction_Manager m;

  // true if we want to include changeset discussions along with
  // the changesets themselves. defaults to false.
  bool include_changeset_discussions;

  // true if the user is a moderator and we should include redacted historical
  // versions in the responses.
  bool m_redactions_visible;

  // the set of selected nodes, ways and relations
  std::set<osm_changeset_id_t> sel_changesets;
  std::set<osm_nwr_id_t> sel_nodes, sel_ways, sel_relations;
  std::set<osm_edition_t> sel_historic_nodes, sel_historic_ways, sel_historic_relations;
  std::map<osm_changeset_id_t, changeset> cc;
};

#endif /* READONLY_PGSQL_SELECTION_HPP */
