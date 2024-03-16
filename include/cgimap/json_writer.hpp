/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef JSON_WRITER_HPP
#define JSON_WRITER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>
#include <yajl/yajl_gen.h>
#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"

/**
 * nice(ish) interface to writing a JSON file.
 */
class json_writer : public output_writer {
public:
  json_writer(const json_writer &) = delete;
  json_writer& operator=(const json_writer &) = delete;
  json_writer(json_writer &&) = default;

  // create a json writer using a callback object for output
  explicit json_writer(output_buffer &out, bool indent = false);

  // closes and flushes the buffer
  ~json_writer() noexcept override;

  void start_object();
  void object_key(const std::string &s);
  void end_object();

  void start_array();
  void end_array();

  void entry(bool b);
  void entry(double d);

  template<typename TInteger, std::enable_if_t<std::is_integral_v<TInteger>, bool> = true>
  void entry(TInteger i) {
    yajl_gen_integer(gen, i);
  }

  void entry(const char* s);
  void entry(std::string_view sv);

  template <typename TKey, typename TValue>
  void property(TKey&& key, TValue&& val) {
    object_key(std::forward<TKey>(key));
    entry(std::forward<TValue>(val));
  }

  void flush() override;

  void error(const std::string &) override;

private:
  yajl_gen gen;
  yajl_alloc_funcs alloc_funcs;
  output_buffer& out;
};

#endif /* JSON_WRITER_HPP */
