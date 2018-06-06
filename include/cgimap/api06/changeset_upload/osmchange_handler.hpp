#ifndef OSMCHANGE_HANDLER_HPP
#define OSMCHANGE_HANDLER_HPP

#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "node.hpp"
#include "osmobject.hpp"
#include "parser_callback.hpp"
#include "relation.hpp"
#include "way.hpp"

class OSMChange_Handler : public Parser_Callback {

public:
  OSMChange_Handler(std::unique_ptr<Node_Updater> _node_updater,
                    std::unique_ptr<Way_Updater> _way_updater,
                    std::unique_ptr<Relation_Updater> _relation_updater,
                    osm_changeset_id_t _changeset, osm_user_id_t _uid);

  virtual ~OSMChange_Handler() {};

  // checks common to all objects
  void check_osm_object(const OSMObject &o) const;

  void start_document();

  void end_document();

  void process_node(const Node &node, operation op, bool if_unused);

  void process_way(const Way &way, operation op, bool if_unused);

  void process_relation(const Relation &relation, operation op, bool if_unused);

  unsigned int get_num_changes();

  bbox_t get_bbox();

private:
  enum class state {
    st_initial,
    st_create_node,
    st_create_way,
    st_create_relation,
    st_modify,
    st_delete_relation,
    st_delete_way,
    st_delete_node,
    st_finished
  };

  void handle_new_state(state new_state);

  void finish_processing();

  state current_state{ state::st_initial };

  std::unique_ptr<Node_Updater> node_updater;
  std::unique_ptr<Way_Updater> way_updater;
  std::unique_ptr<Relation_Updater> relation_updater;

  osm_changeset_id_t m_changeset;
  osm_user_id_t m_uid;
};

#endif
