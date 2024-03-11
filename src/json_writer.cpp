/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <yajl/yajl_gen.h>
#include <memory>
#include <cstdio>
#include <cstring>
#include <utility>
#include <array>
#include <fmt/core.h>
#include <fmt/compile.h>

#include "cgimap/json_writer.hpp"

struct json_writer::pimpl_ {
  // not sure whether the config.hppas to live as long as the generator itself,
  // so seems best to be on the safe side.
  yajl_gen gen;

#if HAVE_YAJL2
  yajl_alloc_funcs alloc_funcs;
#else
  yajl_gen_config config;
#endif
};

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
    : pimpl(std::make_unique<pimpl_>()), out(out) {
#if HAVE_YAJL2
  pimpl->gen = yajl_gen_alloc(NULL);

#else  /* older version of YAJL */
  // setup whether the generator should produce pretty output
  if (indent) {
    pimpl->config.beautify = 1;
    pimpl->config.indentString = " ";
  } else {
    pimpl->config.beautify = 0;
    pimpl->config.indentString = "";
  }

  pimpl->gen = yajl_gen_alloc2(&wrap_write, &pimpl->config, NULL, out.get());
#endif /* HAVE_YAJL2 */

  if (pimpl->gen == 0) {
    throw std::runtime_error("error creating json writer.");
  }

#if HAVE_YAJL2
  if (indent) {
    yajl_gen_config(pimpl->gen, yajl_gen_beautify, 1);
    yajl_gen_config(pimpl->gen, yajl_gen_indent_string, " ");
  } else {
    yajl_gen_config(pimpl->gen, yajl_gen_beautify, 0);
    yajl_gen_config(pimpl->gen, yajl_gen_indent_string, "");
  }
  yajl_gen_config(pimpl->gen, yajl_gen_print_callback, &wrap_write,
                  (void *)&out);
#endif /* HAVE_YAJL2 */
}

json_writer::~json_writer() noexcept {
  yajl_gen_clear(pimpl->gen);
  yajl_gen_free(pimpl->gen);

  try {
    out.close();
  } catch (...) {
    // don't do anything here or we risk FUBARing the entire program.
    // it might not be possible to end the document because the output
    // stream went away. if so, then there is nothing to do but try
    // and reclaim the extra memory.
  }

}

void json_writer::start_object() { yajl_gen_map_open(pimpl->gen); }

void json_writer::object_key(const std::string &s) {
  yajl_gen_string(pimpl->gen, (const unsigned char *)s.c_str(), s.size());
}

void json_writer::end_object() { yajl_gen_map_close(pimpl->gen); }

void json_writer::entry_bool(bool b) { yajl_gen_bool(pimpl->gen, b ? 1 : 0); }

void json_writer::entry_int(int32_t i) { yajl_gen_integer(pimpl->gen, i); }

void json_writer::entry_int(int64_t i) { yajl_gen_integer(pimpl->gen, i); }

void json_writer::entry_int(uint32_t i) { yajl_gen_integer(pimpl->gen, i); }

void json_writer::entry_int(uint64_t i) { yajl_gen_integer(pimpl->gen, i); }

void json_writer::entry_double(double d) {
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

  yajl_gen_number(pimpl->gen, str, len);
}

void json_writer::entry_string(const std::string &s) {
  yajl_gen_string(pimpl->gen, (const unsigned char *)s.c_str(), s.size());
}

void json_writer::start_array() { yajl_gen_array_open(pimpl->gen); }

void json_writer::end_array() { yajl_gen_array_close(pimpl->gen); }

void json_writer::flush() { yajl_gen_clear(pimpl->gen); }

void json_writer::error(const std::string &s) {
  start_object();
  object_key("error");
  entry_string(s);
  end_object();
}
