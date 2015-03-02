#include "cgimap/output_formatter.hpp"

element_info::element_info()
  : id(0), version(0), changeset(0),
    timestamp(), uid(), display_name(),
    visible(false) {}

element_info::element_info(const element_info &other)
  : id(other.id), version(other.version), changeset(other.changeset),
    timestamp(other.timestamp), uid(other.uid), display_name(other.display_name),
    visible(other.visible) {}

element_info::element_info(osm_id_t id_, osm_id_t version_, osm_id_t changeset_,
                           const std::string &timestamp_,
                           const boost::optional<osm_id_t> &uid_,
                           const boost::optional<std::string> &display_name_,
                           bool visible_)
  : id(id_), version(version_), changeset(changeset_),
    timestamp(timestamp_), uid(uid_), display_name(display_name_),
    visible(visible_) {}

output_formatter::~output_formatter() {}
