/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include <cstdio>
#include <cstring>
#include <array>
#include <fmt/core.h>
#include <fmt/compile.h>

#include "cgimap/json_writer.hpp"


json_writer::json_writer(output_buffer &out, bool indent)
    : out(out) {

  gen = yajl_gen_alloc(nullptr);

  if (gen == nullptr) {
    throw std::runtime_error("error creating json writer.");
  }

  if (indent) {
    yajl_gen_config(gen, yajl_gen_beautify, 1);
    yajl_gen_config(gen, yajl_gen_indent_string, " ");
  } else {
    yajl_gen_config(gen, yajl_gen_beautify, 0);
    yajl_gen_config(gen, yajl_gen_indent_string, "");
  }
}

json_writer::~json_writer() noexcept {

  try {
    // there should be nothing left to do, except clearing the yajl buffer
    output_yajl_buffer(true);
  } catch (...) {
    // ignore
  }

  yajl_gen_free(gen);

  try {
    out.close();
  } catch (...) {
    // don't do anything here or we risk FUBARing the entire program.
    // it might not be possible to end the document because the output
    // stream went away. if so, then there is nothing to do but try
    // and reclaim the extra memory.
  }

}

void json_writer::start_object() {
  yajl_gen_map_open(gen);
}

void json_writer::object_key(std::string_view sv) {
  entry(sv);
}

void json_writer::end_object() {
  yajl_gen_map_close(gen);
  output_yajl_buffer(false);
}

void json_writer::entry(bool b) {
  yajl_gen_bool(gen, b ? 1 : 0);
}

void json_writer::entry(double d) {
  const char* str = nullptr;
  size_t len = 0;

#if FMT_VERSION >= 90000
  std::array<char, 384> buf;
  constexpr size_t max_chars = buf.size() - 1;
  auto [end, n_written] = fmt::format_to_n(buf.begin(), max_chars, FMT_COMPILE("{:.7f}"), d);
  if (n_written > max_chars)
    throw write_error("cannot convert double-precision attribute to string.");
  *end = '\0'; // Null terminate string
  str = buf.data();
  len = n_written;
#else
  auto s = fmt::format("{:.7f}", d);
  str = s.c_str();
  len = s.length();
#endif

  yajl_gen_number(gen, str, len);
}

void json_writer::start_array() {
  yajl_gen_array_open(gen);
}

void json_writer::end_array() {
  yajl_gen_array_close(gen);
  output_yajl_buffer(false);
}

void json_writer::flush() {
  output_yajl_buffer(true);
}

void json_writer::error(const std::string &s) {
  start_object();
  property("error", s);
  end_object();

  output_yajl_buffer(true);
}

void json_writer::output_yajl_buffer(bool ignore_buffer_size)
{
  const unsigned char *yajl_buf = nullptr;
  size_t yajl_buf_len = 0;

  if (yajl_gen_get_buf(gen, &yajl_buf, &yajl_buf_len) != yajl_gen_status_ok)
    throw output_writer::write_error("Expected yajl_gen_status_ok");

  // Keep adding more JSON elements, if the yajl buffer size hasn't
  // reached our threshold value yet.
  // Setting ignore_buffer_size to true will skip this check.
  if (!ignore_buffer_size && yajl_buf_len < MAX_BUFFER)
    return;

  // empty yajl buffer -> don't send anything to out
  if (yajl_buf_len != 0) {

    // Write yajl buffer to output
    int wrote_len = out.write((const char*) yajl_buf, yajl_buf_len);

    if (wrote_len != int(yajl_buf_len)) {
      throw output_writer::write_error(
          "Output buffer wrote a different amount than was expected.");
    }
  }

  // clear YAJL internal buffer
  yajl_gen_clear(gen);
}
