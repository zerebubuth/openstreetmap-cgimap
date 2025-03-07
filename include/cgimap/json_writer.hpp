/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef JSON_WRITER_HPP
#define JSON_WRITER_HPP

#include <array>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/compile.h>
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
  void object_key(std::string_view sv);
  void end_object();

  void start_array();
  void end_array();

  void entry(bool b);
  void entry(double d);

  template<typename TInteger, std::enable_if_t<std::is_integral_v<TInteger>, bool> = true>
  void entry(TInteger i) {

    const char* str = nullptr;
    size_t len = 0;

  #if FMT_VERSION >= 90000
    std::array<char, 64> buf;
    constexpr size_t max_chars = buf.size() - 1;
    auto [end, n_written] = fmt::format_to_n(buf.begin(), max_chars, FMT_COMPILE("{:d}"), i);
    if (n_written > max_chars)
      throw write_error("cannot convert int attribute to string.");
    *end = '\0'; // Null terminate string
    str = buf.data();
    len = n_written;
  #else
    auto s = fmt::format("{:d}", i);
    str = s.c_str();
    len = s.length();
  #endif

    yajl_gen_number(gen, str, len);
  }

  template <typename T,
            std::enable_if_t<std::is_convertible_v<T&&, std::string_view>,
                             bool> = true>
  void entry(T&& s)
  {
    auto sv = std::string_view(s);
    yajl_gen_string(gen, (const unsigned char *)sv.data(), sv.size());
  }

  template <typename TKey, typename TValue>
  void property(TKey&& key, TValue&& val) {
    object_key(std::forward<TKey>(key));
    entry(std::forward<TValue>(val));
  }

  void flush() override;

  void error(const std::string &) override;

private:
  void output_yajl_buffer(bool ignore_buffer_size);

  yajl_gen gen;
  output_buffer& out;

  constexpr static int MAX_BUFFER = 16384;
};

#endif /* JSON_WRITER_HPP */
