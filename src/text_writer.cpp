/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include "cgimap/text_writer.hpp"


// create a new text writer using writer callback functions
text_writer::text_writer(output_buffer &o, bool indent) : out(o) {

}


text_writer::~text_writer() noexcept {

  try {
    out.close();
  } catch (...) {
    // don't do anything here or we risk FUBARing the entire program.
    // it might not be possible to end the document because the output
    // stream went away. if so, then there is nothing to do but try
    // and reclaim the extra memory.
  }

}

void text_writer::start(const std::string &name) {

}


void text_writer::text(const std::string &t) {
  out.write(t.c_str(), t.size());
}

void text_writer::end() {

}

void text_writer::flush() {
  out.flush();

}

void text_writer::error(const std::string &s) {
  if (!s.empty())
    text(s);
}

