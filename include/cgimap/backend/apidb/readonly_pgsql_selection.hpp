#ifndef READONLY_PGSQL_SELECTION_HPP
#define READONLY_PGSQL_SELECTION_HPP

#include "cgimap/data_selection.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/backend/apidb/cache.hpp"
#include <pqxx/pqxx>
#include <boost/program_options.hpp>
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
  readonly_pgsql_selection(pqxx::connection &conn,
                           cache<osm_changeset_id_t, changeset> &changeset_cache);
  ~readonly_pgsql_selection();

  void write_nodes(output_formatter &formatter);
  void write_ways(output_formatter &formatter);
  void write_relations(output_formatter &formatter);
  void write_changesets(output_formatter &formatter, const boost::posix_time::ptime &now);

  visibility_t check_node_visibility(osm_nwr_id_t id);
  visibility_t check_way_visibility(osm_nwr_id_t id);
  visibility_t check_relation_visibility(osm_nwr_id_t id);

  int select_nodes(const std::vector<osm_nwr_id_t> &);
  int select_ways(const std::vector<osm_nwr_id_t> &);
  int select_relations(const std::vector<osm_nwr_id_t> &);
  int select_nodes_from_bbox(const bbox &bounds, int max_nodes);
  void select_nodes_from_relations();
  void select_ways_from_nodes();
  void select_ways_from_relations();
  void select_relations_from_ways();
  void select_nodes_from_way_nodes();
  void select_relations_from_nodes();
  void select_relations_from_relations();
  void select_relations_members_of_relations();

  bool supports_changesets();
  int select_changesets(const std::vector<osm_changeset_id_t> &);
  void select_changeset_discussions();

  bool supports_historical_versions();
  int select_historical_nodes(const std::vector<osm_edition_t> &);
  int select_historical_ways(const std::vector<osm_edition_t> &);
  int select_historical_relations(const std::vector<osm_edition_t> &);
  int select_nodes_with_history(const std::vector<osm_nwr_id_t> &);
  int select_ways_with_history(const std::vector<osm_nwr_id_t> &);
  int select_relations_with_history(const std::vector<osm_nwr_id_t> &);
  void set_redactions_visible(bool);
  int select_historical_by_changesets(const std::vector<osm_changeset_id_t> &);

  /**
   * a factory for the creation of read-only selections, so it
   * can set up prepared statements.
   */
  class factory : public data_selection::factory {
  public:
    factory(const boost::program_options::variables_map &);
    virtual ~factory();
    virtual boost::shared_ptr<data_selection> make_selection();

  private:
    pqxx::connection m_connection, m_cache_connection;
#if PQXX_VERSION_MAJOR >= 4
    pqxx::quiet_errorhandler m_errorhandler, m_cache_errorhandler;
#endif
    pqxx::nontransaction m_cache_tx;
    cache<osm_changeset_id_t, changeset> m_cache;
  };

private:
  // the transaction in which the selection takes place. this is
  // fully read-only, and cannot create any temporary tables,
  // unlike writeable_pgsql_selection.
  pqxx::work w;

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
  cache<osm_changeset_id_t, changeset> &cc;
};

#endif /* READONLY_PGSQL_SELECTION_HPP */
