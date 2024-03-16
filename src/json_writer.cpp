/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include <memory>
#include <cstdio>
#include <cstring>
#include <utility>
#include <array>
#include <fmt/core.h>
#include <fmt/compile.h>

#include "cgimap/json_writer.hpp"

static void wrap_write(void *context, const char *str, unsigned int len) {
  auto *out = static_cast<output_buffer *>(context);
  if (out == 0) {
    throw output_writer::write_error(
        "Output buffer was NULL in json_writer wrap_write().");
  }

  int wrote_len = out->write(str, len);

  if (wrote_len != int(len)) {
    throw output_writer::write_error(
        "Output buffer wrote a different amount than was expected.");
  }
}

json_writer::json_writer(output_buffer &out, bool indent)
    : out(out) {

  gen = yajl_gen_alloc(NULL);

  if (gen == 0) {
    throw std::runtime_error("error creating json writer.");
  }

  if (indent) {
    yajl_gen_config(gen, yajl_gen_beautify, 1);
    yajl_gen_config(gen, yajl_gen_indent_string, " ");
  } else {
    yajl_gen_config(gen, yajl_gen_beautify, 0);
    yajl_gen_config(gen, yajl_gen_indent_string, "");
  }
  yajl_gen_config(gen, yajl_gen_print_callback, &wrap_write,
                  (void *)&out);
}

json_writer::~json_writer() noexcept {
  yajl_gen_clear(gen);
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

void json_writer::start_object() { yajl_gen_map_open(gen); }

void json_writer::object_key(const std::string &s) {
  entry(s);
}

void json_writer::end_object() { yajl_gen_map_close(gen); }

void json_writer::entry(bool b) { yajl_gen_bool(gen, b ? 1 : 0); }

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

void json_writer::entry(const char* s) {
  entry(std::string_view(s));
}

void json_writer::entry(std::string_view sv) {
  yajl_gen_string(gen, (const unsigned char *)sv.data(), sv.size());
}

void json_writer::start_array() { yajl_gen_array_open(gen); }

void json_writer::end_array() { yajl_gen_array_close(gen); }

void json_writer::flush() { yajl_gen_clear(gen); }

void json_writer::error(const std::string &s) {
  start_object();
  property("error", s);
  end_object();
}
