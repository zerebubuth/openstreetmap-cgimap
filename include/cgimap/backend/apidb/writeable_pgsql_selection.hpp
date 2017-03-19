#ifndef WRITEABLE_PGSQL_SELECTION_HPP
#define WRITEABLE_PGSQL_SELECTION_HPP

#include "cgimap/data_selection.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/backend/apidb/cache.hpp"
#include <pqxx/pqxx>
#include <boost/program_options.hpp>

/**
 * a selection which operates against a writeable (i.e: non read-only
 * slave) PostgreSQL database, such as the rails_port database or
 * an osmosis imported database.
 */
class writeable_pgsql_selection : public data_selection {
public:
  writeable_pgsql_selection(pqxx::connection &conn,
                            cache<osm_changeset_id_t, changeset> &changeset_cache);
  ~writeable_pgsql_selection();

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
   * abstracts the creation of transactions for the writeable
   * data selection.
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
  // the transaction in which the selection takes place. although
  // this *is* read-only, it may create temporary tables.
  pqxx::work w;

  cache<osm_changeset_id_t, changeset> cc;

  // true if a query hasn't been run yet, i.e: it's possible to
  // assume that all the temporary tables are empty.
  bool m_tables_empty;
  bool m_historic_tables_empty;

  // true if we want to include changeset discussions along with
  // the changesets themselves. defaults to false.
  bool include_changeset_discussions;

  // true if redacted historical versions should be included in
  // the query output.
  bool m_redactions_visible;
};

#endif /* WRITEABLE_PGSQL_SELECTION_HPP */
