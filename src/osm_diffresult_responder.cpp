/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/osm_diffresult_responder.hpp"
#include "cgimap/logger.hpp"

#include <chrono>
#include <fmt/core.h>


namespace {

  element_type as_elem_type(object_type o) { // @suppress("No return")

    switch (o) {

      case object_type::node:
	return element_type::node;
      case object_type::way:
	return element_type::way;
      case object_type::relation:
	return element_type::relation;

    }

    throw std::runtime_error("Unhandled object_type in as_elem_type.");
  }

}

osm_diffresult_responder::osm_diffresult_responder(mime::type mt)
    : osm_responder(mt) {}

osm_diffresult_responder::~osm_diffresult_responder() = default;

void osm_diffresult_responder::write(output_formatter& fmt,
                                     const std::string &generator,
                                     const std::chrono::system_clock::time_point &) {


  try {
    fmt.start_document(generator, "diffResult");

    // Iterate over all elements in the sequence defined in the osmChange
    // message
    for (const auto &item : m_diffresult) {

      switch (item.op) {
      case operation::op_create:
      case operation::op_modify:
        fmt.write_diffresult_create_modify(
            as_elem_type(item.obj_type), item.old_id,
            item.new_id, item.new_version);

        break;

      case operation::op_delete:
        if (item.deletion_skipped)
          fmt.write_diffresult_create_modify(
              as_elem_type(item.obj_type), item.old_id,
              item.new_id, item.new_version);
        else
          fmt.write_diffresult_delete(as_elem_type(item.obj_type),
                                      item.old_id);
        break;

      case operation::op_undefined:

	break;
      }
    }

  } catch (const std::exception &e) {
    logger::message(fmt::format("Caught error in osm_diffresult_responder: {}",
                        e.what()));
    fmt.error(e);
  }

  fmt.end_document();
}
