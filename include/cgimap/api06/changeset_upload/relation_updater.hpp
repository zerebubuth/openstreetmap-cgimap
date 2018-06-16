#ifndef API06_CHANGESET_UPLOAD_RELATION_UPDATER
#define API06_CHANGESET_UPLOAD_RELATION_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/relation.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

using RelationMemberList = std::vector<RelationMember>;
using TagList = std::map<std::string, std::string>;

class Relation_Updater {

public:
  virtual ~Relation_Updater() {};

  virtual void add_relation(osm_changeset_id_t changeset_id,
                            osm_nwr_signed_id_t old_id,
                            const RelationMemberList &members,
                            const TagList &tags) = 0;

  virtual void modify_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                               osm_version_t version,
                               const RelationMemberList &members,
                               const TagList &tags) = 0;

  virtual void delete_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                               osm_version_t version, bool if_unused) = 0;

  virtual void process_new_relations() = 0;

  virtual void process_modify_relations() = 0;

  virtual void process_delete_relations() = 0;

  virtual uint32_t get_num_changes() = 0;

  virtual bbox_t bbox() = 0;
};

#endif /* API06_CHANGESET_UPLOAD_RELATION_UPDATER */
