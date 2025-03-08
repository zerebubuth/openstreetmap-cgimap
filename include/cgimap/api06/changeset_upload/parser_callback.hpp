/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_CHANGESET_UPLOAD_PARSER_CALLBACK_HPP
#define API06_CHANGESET_UPLOAD_PARSER_CALLBACK_HPP

#include "node.hpp"
#include "relation.hpp"
#include "cgimap/types.hpp"
#include "way.hpp"


namespace api06 {

class Parser_Callback {

public:
  virtual ~Parser_Callback() = default;

  virtual void start_document() = 0;

  virtual void end_document() = 0;

  virtual void process_node(const Node &, operation op, bool if_unused) = 0;

  virtual void process_way(const Way &, operation op, bool if_unused) = 0;

  virtual void process_relation(const Relation &, operation op, bool if_unused) = 0;
};

} // namespace api06

#endif
