#ifndef API06_CHANGESET_UPLOAD_PARSER_CALLBACK_HPP
#define API06_CHANGESET_UPLOAD_PARSER_CALLBACK_HPP

#include "node.hpp"
#include "osmobject.hpp"
#include "relation.hpp"
#include "cgimap/types.hpp"
#include "way.hpp"


namespace api06 {

class Parser_Callback {

public:

  virtual void start_document() = 0;

  virtual void end_document() = 0;

  virtual void process_node(const Node &, operation op, bool if_unused) = 0;

  virtual void process_way(const Way &, operation op, bool if_unused) = 0;

  virtual void process_relation(const Relation &, operation op, bool if_unused) = 0;

  virtual ~Parser_Callback() = default;
};

} // namespace api06

#endif
