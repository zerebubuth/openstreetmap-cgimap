#ifndef OUTPUT_FORMATTER_HPP
#define OUTPUT_FORMATTER_HPP

#include "cgimap/bbox.hpp"
#include "cgimap/types.hpp"
#include "cgimap/mime_types.hpp"
#include <list>
#include <vector>
#include <stdexcept>
#include <boost/optional.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

/**
 * What type of element the formatter is starting to write.
 */
enum element_type {
  element_type_changeset,
  element_type_node,
  element_type_way,
  element_type_relation
};

// TODO: document me.
enum action_type {
  action_type_create,
  action_type_modify,
  action_type_delete
};

struct element_info {
  element_info();
  element_info(const element_info &);
  element_info(osm_nwr_id_t id_, osm_nwr_id_t version_,
               osm_changeset_id_t changeset_,
               const std::string &timestamp_,
               const boost::optional<osm_user_id_t> &uid_,
               const boost::optional<std::string> &display_name_,
               bool visible_,
               boost::optional<osm_redaction_id_t> redaction_ = boost::none);
  // Standard meanings
  osm_nwr_id_t id, version;
  osm_changeset_id_t changeset;
  std::string timestamp;
  // Anonymous objects will not have uids or display names
  boost::optional<osm_user_id_t> uid;
  boost::optional<std::string> display_name;
  // If an object has been deleted
  bool visible;
  // If an object has administratively hidden in a "redaction". note that this
  // is never output - if it is present, then the element should not be
  // displayed except to moderators.
  boost::optional<osm_redaction_id_t> redaction;
};

struct changeset_info {
  changeset_info();
  changeset_info(const changeset_info &);
  changeset_info(osm_changeset_id_t id_,
                 const std::string &created_at_,
                 const std::string &closed_at_,
                 const boost::optional<osm_user_id_t> &uid_,
                 const boost::optional<std::string> &display_name_,
                 const boost::optional<bbox> &bounding_box_,
                 size_t num_changes_,
                 size_t comments_count_);

  // returns true if the changeset is "open" at a particular
  // point in time.
  //
  // note that the definition of "open" is fraught with
  // difficulty, and it's not wise to rely on it too much.
  bool is_open_at(const boost::posix_time::ptime &) const;

  // standard meaning of ID
  osm_changeset_id_t id;
  // changesets are created at a certain time and may be either
  // closed explicitly with a closing time, or close implicitly
  // an hour after the last update to the changeset. closed_at
  // should have an ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
  std::string created_at, closed_at;
  // anonymous objects don't have UIDs or display names
  boost::optional<osm_user_id_t> uid;
  boost::optional<std::string> display_name;
  // changesets with edits will have a bounding box containing
  // the extent of all the changes.
  boost::optional<bbox> bounding_box;
  // the number of changes (new element versions) associated
  // with this changeset.
  size_t num_changes;
  // if the changeset has a discussion attached, then this will
  // be the number of comments.
  size_t comments_count;
};

struct changeset_comment_info {
  osm_user_id_t author_id;
  std::string body;
  std::string created_at;
  std::string author_display_name;

  bool operator==(const changeset_comment_info &) const;
};

struct member_info {
  element_type type;
  osm_nwr_id_t ref;
  std::string role;

  member_info() {}
  member_info(element_type type_, osm_nwr_id_t ref_, const std::string &role_)
    : type(type_), ref(ref_), role(role_) {}

  inline bool operator==(const member_info &other) const {
    return ((type == other.type) &&
            (ref == other.ref) &&
            (role == other.role));
  }
};

typedef std::list<osm_nwr_id_t> nodes_t;
typedef std::list<member_info> members_t;
typedef std::list<std::pair<std::string, std::string> > tags_t;
typedef std::vector<changeset_comment_info> comments_t;

/**
 * Base type for different output formats. Hopefully this is general
 * enough to encompass most formats that we want to produce. Assuming,
 * of course, that we want any other formats ;-)
 */
struct output_formatter {
  virtual ~output_formatter();

  // returns the mime type of the content that this formatter will
  // produce.
  virtual mime::type mime_type() const = 0;

  // called once to start the document - this will be the first call to this
  // object after construction. the first argument will be used as the
  // "generator" header attribute, and the second will name the root element
  // (if there is one - JSON doesn't have one), e.g: "osm" or "osmChange".
  virtual void start_document(
    const std::string &generator, const std::string &root_name) = 0;

  // called once to end the document - there will be no calls after this
  // one. this will be called, even if an error has occurred.
  virtual void end_document() = 0;

  // this is called if there is an error during reading data from the
  // database. hopefully this is a very very rare occurrance.
  virtual void error(const std::exception &e) = 0;

  // write a bounds object to the document. this seems to be generally used
  // to record the box used by a map call.
  virtual void write_bounds(const bbox &bounds) = 0;

  // start a type of element. this is called once for nodes, ways or
  // relations. between the start and end called for a particular element
  // type only write_* functions for that type will be called.
  virtual void start_element_type(element_type type) = 0;

  // end a type of element. this is called once for nodes, ways or relations
  virtual void end_element_type(element_type type) = 0;

  // TODO: document me.
  virtual void start_action(action_type type) = 0;
  virtual void end_action(action_type type) = 0;

  // output a single node given that node's row and an iterator over its tags
  virtual void write_node(const element_info &elem, double lon, double lat,
                          const tags_t &tags) = 0;

  // output a single way given a row and iterators for nodes and tags
  virtual void write_way(const element_info &elem, const nodes_t &nodes,
                         const tags_t &tags) = 0;

  // output a single relation given a row and iterators over members and tags
  virtual void write_relation(const element_info &elem,
                              const members_t &members, const tags_t &tags) = 0;

  // output a single changeset.
  virtual void write_changeset(const changeset_info &elem,
                               const tags_t &tags,
                               bool include_comments,
                               const comments_t &comments,
                               const boost::posix_time::ptime &now) = 0;

  // flush the current state
  virtual void flush() = 0;

  // write an error to the output stream
  virtual void error(const std::string &) = 0;
};

#endif /* OUTPUT_FORMATTER_HPP */
