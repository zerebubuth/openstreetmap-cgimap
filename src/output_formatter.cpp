#include "cgimap/output_formatter.hpp"

element_info::element_info()
  : id(0), version(0), changeset(0),
    timestamp(), uid(), display_name(),
    visible(false) {}

element_info::element_info(const element_info &other)
  : id(other.id), version(other.version), changeset(other.changeset),
    timestamp(other.timestamp), uid(other.uid), display_name(other.display_name),
    visible(other.visible) {}

element_info::element_info(osm_nwr_id_t id_, osm_nwr_id_t version_,
                           osm_changeset_id_t changeset_,
                           const std::string &timestamp_,
                           const boost::optional<osm_user_id_t> &uid_,
                           const boost::optional<std::string> &display_name_,
                           bool visible_)
  : id(id_), version(version_), changeset(changeset_),
    timestamp(timestamp_), uid(uid_), display_name(display_name_),
    visible(visible_) {}

changeset_info::changeset_info()
  : id(0), created_at(""), closed_at(""), uid(),
    display_name(), bounding_box(), num_changes(0),
    comments_count(0), open(false) {}

changeset_info::changeset_info(const changeset_info &other)
  : id(other.id), created_at(other.created_at),
    closed_at(other.closed_at), uid(other.uid),
    display_name(other.display_name), bounding_box(other.bounding_box),
    num_changes(other.num_changes), comments_count(other.comments_count),
    open(other.open) {}

changeset_info::changeset_info(
  osm_changeset_id_t id_,
  const std::string &created_at_,
  const std::string &closed_at_,
  const boost::optional<osm_user_id_t> &uid_,
  const boost::optional<std::string> &display_name_,
  const boost::optional<bbox> &bounding_box_,
  size_t num_changes_,
  size_t comments_count_,
  bool open_)
  : id(id_), created_at(created_at_), closed_at(closed_at_),
    uid(uid_), display_name(display_name_),
    bounding_box(bounding_box_), num_changes(num_changes_),
    comments_count(comments_count_), open(open_) {}

output_formatter::~output_formatter() {}
