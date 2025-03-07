/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"

#include <sstream>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

namespace {

  std::string repeat(const std::string &input, size_t num) {
    std::ostringstream os;
    std::fill_n(std::ostream_iterator<std::string>(os), num, input);
    return os.str();
  }

}

std::map<std::string, std::string> process_testmsg(const std::string &payload) {

  std::setlocale(LC_ALL, "C.UTF-8");
  api06::ChangesetXMLParser parser{};
  return parser.process_message(payload);
}


TEST_CASE("Invalid XML", "[changeset]") {
  auto i = GENERATE(R"(<osm>)",
                    R"(<invalid>)",
                    R"(bla)",
                    R"(<osm><invalid/></osm>)",
                    R"(<osm><changeset><invalid/></changeset></osm>)",
                    R"(<osm><changeset><tag/></changeset></osm>)",
                    "");
  REQUIRE_THROWS_AS(process_testmsg(i), http::bad_request);
}


TEST_CASE("Invalid XML - empty key", "[changeset]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osm><changeset><tag k="" v="val"/></changeset></osm>)"), http::bad_request);
}

TEST_CASE("Missing changeset tag", "[changeset]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osm/>)"), http::bad_request);
}

TEST_CASE("Changeset without tags", "[changeset]") {
  std::map<std::string, std::string> tags;
  REQUIRE_NOTHROW(tags = process_testmsg(R"(<osm><changeset/></osm>)"));
  CHECK(tags.empty());
}

TEST_CASE("Changeset with multiple identical tags", "[changeset]") {
   std::map<std::string, std::string> tags;
   REQUIRE_NOTHROW( tags = process_testmsg(R"(<osm>
                     <changeset>
                          <tag k="key1" v="val1"/>
                          <tag k="key2" v="val2"/>
                          <tag k="key1" v="val1NEW"/>
                      </changeset>
                 </osm>)"));

   CHECK(tags.size() == 2);
   CHECK(tags["key1"] == "val1NEW");
}

TEST_CASE("Changeset with multiple changeset tags and identical tags", "[changeset]") {

  std::map<std::string, std::string> tags;
  REQUIRE_NOTHROW( tags = process_testmsg(R"(<osm>
                    <changeset>
                         <tag k="key1" v="val1"/>
                         <tag k="key2" v="val2"/>
                    </changeset>
                    <changeset>
                         <tag k="key1" v="val1NEW"/>
                     </changeset>
                  </osm>)"));

  CHECK(tags.size() == 2);
  CHECK(tags["key1"] == "val1NEW");
}

TEST_CASE("Tag: Key without value", "[changeset]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osm><changeset><tag k="key"/></changeset></osm>)"), http::bad_request);
}

TEST_CASE("Tag: Value without key", "[changeset]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osm><changeset><tag v="value"/></changeset></osm>)"), http::bad_request);
}

TEST_CASE("Tag: Key with max 255 unicode characters, <= 255", "[changeset]") {
  for (int i = 1; i <= 255; i++) {
    auto v = repeat("😎", i);
    REQUIRE_NOTHROW(process_testmsg(
        fmt::format( R"(<osm><changeset><tag k="{}" v="value"/></changeset></osm>)", v)));
  }
}

TEST_CASE("Tag: Key with max 255 unicode characters, > 255", "[changeset]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
      fmt::format( R"(<osm><changeset><tag k="{}" v="value"/></changeset></osm>)", repeat("😎", 256))),
    http::bad_request, Catch::Message("Key has more than 255 unicode characters at line 1, column 292"));
}


TEST_CASE("Tag: Value with max 255 unicode characters, <= 255", "[changeset]") {
  for (int i = 1; i <= 255; i++) {
    auto v = repeat("😎", i);
    REQUIRE_NOTHROW(process_testmsg(
        fmt::format( R"(<osm><changeset><tag k="key" v="{}"/></changeset></osm>)", v)));
  }
}

TEST_CASE("Tag: Value with max 255 unicode characters, > 255", "[changeset]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
      fmt::format( R"(<osm><changeset><tag k="key" v="{}"/></changeset></osm>)", repeat("😎", 256))),
    http::bad_request, Catch::Message("Value has more than 255 unicode characters at line 1, column 290"));
}

