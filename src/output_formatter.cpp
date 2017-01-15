#include "cgimap/output_formatter.hpp"
#include "cgimap/time.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace pt = boost::posix_time;

// maximum number of element versions which can be associated
// with a single changeset. this is hard-coded in the API,
// see app/models/changeset.rb for the details.
#define MAX_CHANGESET_ELEMENTS (50000)

element_info::element_info()
  : id(0), version(0), changeset(0),
    timestamp(), uid(), display_name(),
    visible(false), redaction(boost::none) {}

element_info::element_info(const element_info &other)
  : id(other.id), version(other.version), changeset(other.changeset),
    timestamp(other.timestamp), uid(other.uid), display_name(other.display_name),
    visible(other.visible), redaction(other.redaction) {}

element_info::element_info(osm_nwr_id_t id_, osm_nwr_id_t version_,
                           osm_changeset_id_t changeset_,
                           const std::string &timestamp_,
                           const boost::optional<osm_user_id_t> &uid_,
                           const boost::optional<std::string> &display_name_,
                           bool visible_,
                           boost::optional<osm_redaction_id_t> redaction_)
  : id(id_), version(version_), changeset(changeset_),
    timestamp(timestamp_), uid(uid_), display_name(display_name_),
    visible(visible_), redaction(redaction_) {}

changeset_info::changeset_info()
  : id(0), created_at(""), closed_at(""), uid(),
    display_name(), bounding_box(), num_changes(0),
    comments_count(0) {}

changeset_info::changeset_info(const changeset_info &other)
  : id(other.id), created_at(other.created_at),
    closed_at(other.closed_at), uid(other.uid),
    display_name(other.display_name), bounding_box(other.bounding_box),
    num_changes(other.num_changes),
    comments_count(other.comments_count) {}

changeset_info::changeset_info(
  osm_changeset_id_t id_,
  const std::string &created_at_,
  const std::string &closed_at_,
  const boost::optional<osm_user_id_t> &uid_,
  const boost::optional<std::string> &display_name_,
  const boost::optional<bbox> &bounding_box_,
  size_t num_changes_,
  size_t comments_count_)
  : id(id_), created_at(created_at_), closed_at(closed_at_),
    uid(uid_), display_name(display_name_),
    bounding_box(bounding_box_), num_changes(num_changes_),
    comments_count(comments_count_) {}

bool changeset_info::is_open_at(const pt::ptime &now) const {
  const pt::ptime closed_at_time = parse_time(closed_at);
  return (closed_at_time > now) && (num_changes < MAX_CHANGESET_ELEMENTS);
}

bool changeset_comment_info::operator==(const changeset_comment_info &other) const {
  return ((author_id == other.author_id) &&
          (body == other.body) &&
          (created_at == other.created_at) &&
          (author_display_name == other.author_display_name));
}

output_formatter::~output_formatter() {}
