/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include "cgimap/options.hpp"
#include "cgimap/api06/changeset_upload/osmchange_json_input_format.hpp"
#include "cgimap/api06/changeset_upload/parser_callback.hpp"
#include "cgimap/util.hpp"
#include "cgimap/http.hpp"

#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

class Test_Parser_Callback : public api06::Parser_Callback {

public:
  Test_Parser_Callback() = default;

  void start_document() override { start_executed = true; }

  void end_document() override {
    end_executed = true;
    REQUIRE(nodes.empty());
    REQUIRE(ways.empty());
    REQUIRE(relations.empty());
  }

  void process_node(const api06::Node &n, operation op, bool if_unused) override {
    REQUIRE(!nodes.empty());

    auto const& [n_expected, op_expected, if_unused_expected] = nodes.front();

    REQUIRE(n_expected == n);
    REQUIRE(op == op_expected);
    REQUIRE(if_unused == if_unused_expected);

    nodes.pop_front();
  }

  void process_way(const api06::Way &w, operation op, bool if_unused) override {
    REQUIRE(!ways.empty());

    auto const& [w_expected, op_expected, if_unused_expected] = ways.front();

    REQUIRE(w_expected == w);
    REQUIRE(op == op_expected);
    REQUIRE(if_unused == if_unused_expected);

    ways.pop_front();
  }

  void process_relation(const api06::Relation &r, operation op, bool if_unused) override {
    REQUIRE(!relations.empty());

    auto const& [r_expected, op_expected, if_unused_expected] = relations.front();

    REQUIRE(r_expected == r);
    REQUIRE(op == op_expected);
    REQUIRE(if_unused == if_unused_expected);

    relations.pop_front();
  }

  bool start_executed{false};
  bool end_executed{false};

  using node_tuple = std::tuple<api06::Node, operation, bool>;
  using way_tuple = std::tuple<api06::Way, operation, bool>;
  using relation_tuple = std::tuple<api06::Relation, operation, bool>;

  std::list< node_tuple > nodes;
  std::list< way_tuple> ways;
  std::list< relation_tuple > relations;
};

class global_settings_test_class : public global_settings_default {

public:

  std::optional<uint32_t> get_relation_max_members() const override {
     return m_relation_max_members;
  }

  std::optional<uint32_t> get_element_max_tags() const override {
     return m_element_max_tags;
  }

  std::optional<uint32_t> m_relation_max_members{};
  std::optional<uint32_t> m_element_max_tags{};

};

std::string repeat(const std::string &input, size_t num) {
  std::ostringstream os;
  std::fill_n(std::ostream_iterator<std::string>(os), num, input);
  return os.str();
}

void process_testmsg(const std::string &payload, Test_Parser_Callback& cb) {

  api06::OSMChangeJSONParser parser(cb);
  parser.process_message(payload);

  REQUIRE(cb.start_executed);
  REQUIRE(cb.end_executed);
}

void process_testmsg(const std::string &payload) {

  Test_Parser_Callback cb{};
  process_testmsg(payload, cb);
}


// OSMCHANGE STRUCTURE TESTS

TEST_CASE("Invalid JSON", "[osmchange][json]") {
  auto i = GENERATE(R"({})", R"(bla)");
  REQUIRE_THROWS_AS(process_testmsg(i), http::bad_request);
}
/*

TEST_CASE("XML without any changes", "[osmchange][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange/>)"));
}

TEST_CASE("Invalid XML: osmchange end only", "[osmchange][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(</osmChange>)"), http::bad_request);
}

TEST_CASE("Misspelled osmchange xml", "[osmchange][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange2/>)"), http::bad_request);
}

TEST_CASE("osmchange: Unknown action", "[osmchange][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(R"(<osmChange><dummy/></osmChange>)"), http::bad_request,
    Catch::Message("Unknown action dummy, choices are create, modify, delete at line 1, column 18"));
}

TEST_CASE("osmchange: Empty create action", "[osmchange][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange><create/></osmChange>)"));
}

TEST_CASE("osmchange: Empty modify action", "[osmchange][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange><modify/></osmChange>)"));
}

TEST_CASE("osmchange: Empty delete action", "[osmchange][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange><delete/></osmChange>)"));
}

TEST_CASE("osmchange: create invalid object", "[osmchange][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(R"(<osmChange><create><bla/></create></osmChange>)"), http::bad_request,
    Catch::Message("Unknown element bla, expecting node, way or relation at line 1, column 24"));
}

*/

// NODE TESTS

TEST_CASE("Create empty node without details", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"({"osmChange": [{ "type": "node", "action": "create"}]})"), http::bad_request);
}

TEST_CASE("Create node, details except changeset info missing", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"({"osmChange": [{ "type": "node", "action": "create", changeset: 1}]})"), http::bad_request);
}

TEST_CASE("Create node, lat lon missing", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"({"osmChange": [{ "type": "node", "action": "create", changeset: 12, id: -1}]})"), http::bad_request);
}

/*
TEST_CASE("Create node, lat missing", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><create><node changeset="858" id="-1" lon="2"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, lon missing", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><create><node changeset="858" id="-1" lat="2"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, lat outside range", "[osmchange][node][json]") {
  auto i = GENERATE(R"(90.01)", R"(-90.01)");
  REQUIRE_THROWS_AS(process_testmsg(fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="{}" lon="2"/></create></osmChange>)", i)), http::bad_request);
}

TEST_CASE("Create node, lon outside range", "[osmchange][node][json]") {
  auto i = GENERATE(R"(180.01)", R"(-180.01)");
  REQUIRE_THROWS_AS(process_testmsg(fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="{}"/></create></osmChange>)", i)), http::bad_request);
}

TEST_CASE("Create node, lat float overflow", "[osmchange][node][json]") {
  auto i = GENERATE(R"(9999999999999999999999999999999999999999999999.01)", R"(-9999999999999999999999999999999999999999999999.01)");
  REQUIRE_THROWS_AS(process_testmsg(fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="{}" lon="2"/></create></osmChange>)", i)), http::bad_request);
}

TEST_CASE("Create node, lon float overflow", "[osmchange][node][json]") {
  auto i = GENERATE(R"(9999999999999999999999999999999999999999999999.01)", R"(-9999999999999999999999999999999999999999999999.01)");
  REQUIRE_THROWS_AS(process_testmsg(fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="{}"/></create></osmChange>)", i)), http::bad_request);
}

TEST_CASE("Create node, lat non-finite float", "[osmchange][node][json]") {
  auto i = GENERATE(R"(nan)", R"(-nan)", R"(Inf), R"(-Inf)");
  REQUIRE_THROWS_AS(process_testmsg(fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="{}" lon="2"/></create></osmChange>)", i)), http::bad_request);
}

TEST_CASE("Create node, lon non-finite float", "[osmchange][node][json]") {
  auto i = GENERATE(R"(nan)", R"(-nan)", R"(Inf), R"(-Inf)");
  REQUIRE_THROWS_AS(process_testmsg(fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="{}"/></create></osmChange>)", i)), http::bad_request);
}

TEST_CASE("Create node, changeset missing", "[osmchange][node][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(R"(<osmChange><create><node id="-1" lat="-90.00" lon="-180.00"/></create></osmChange>)"), http::bad_request,
    Catch::Message("Changeset id is missing for Node -1 at line 1, column 60"));
}

TEST_CASE("Create node, redefined lat attribute", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><create><node changeset="858" id="-1" lat="-90.00" lon="-180.00" lat="20"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create valid node", "[osmchange][node][json]") {
  auto i = GENERATE(R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="180.00"/></create></osmChange>)",
                    R"(<osmChange><create><node changeset="858" id="-1" lat="-90.00" lon="-180.00"/></create></osmChange>)");
  REQUIRE_NOTHROW(process_testmsg(i));
}

TEST_CASE("Modify node, missing version", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><modify><node changeset="858" id="123" lat="90.00" lon="180.00"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Modify node, invalid version", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><modify><node changeset="858" version="0" id="123"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Delete node", "[osmchange][node][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange><delete><node changeset="858" version="1" id="123"/></delete></osmChange>)"));
}

TEST_CASE("Delete node, if-unused", "[osmchange][node][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange><delete if-unused="true"><node changeset="858" version="1" id="123"/></delete></osmChange>)"));
}

TEST_CASE("Delete node, missing version", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><delete><node changeset="858" id="123"/></delete></osmChange>)"), http::bad_request);
}

TEST_CASE("Delete node, invalid version", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><delete><node changeset="858" version="0" id="123"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Delete node, missing id", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(R"(<osmChange><delete><node changeset="858" version="1"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, extra xml nested inside tag", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="-90.00" lon="-180.00">
        <tag k="1" v="2"><blubb/></tag></node></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, empty tag key", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
        <tag k="" v="value"/></node></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, empty tag value", "[osmchange][node][json]") {
  REQUIRE_NOTHROW(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
        <tag k="key" v=""/></node></create></osmChange>)"));
}

TEST_CASE("Create node, duplicate key dup1", "[osmchange][node][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag k="key1" v="value1"/>
                       <tag k="dup1" v="value2"/>
                       <tag k="dup1" v="value3"/>
                       <tag k="key3" v="value4"/>
                       </node></create></osmChange>)"),
    http::bad_request, Catch::Message("Node -1 has duplicate tags with key dup1 at line 4, column 48"));
}

TEST_CASE("Create node, tag without value", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag k="key"/></node></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, tag without key", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag v="value"/></node></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, tag value with <= 255 unicode characters", "[osmchange][node][json]") {
  for (int i = 0; i <= 255; i++) {
    auto v = repeat("😎", i);
    REQUIRE_NOTHROW(process_testmsg(
      fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                            <tag k="key" v="{}"/></node></create></osmChange>)", v)));
  }
}

TEST_CASE("Create node, tag value with > 255 unicode characters", "[osmchange][node][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                           <tag k="key" v="{}"/></node></create></osmChange>)", repeat("😎", 256))),
    http::bad_request, Catch::Message("Value has more than 255 unicode characters in Node -1 at line 2, column 301"));
}

TEST_CASE("Create node, tag key with <= 255 unicode characters", "[osmchange][node][json]") {
  for (int i = 1; i <= 255; i++) {
    auto v = repeat("😎", i);
    REQUIRE_NOTHROW(process_testmsg(
      fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                           <tag k="{}" v="value"/></node></create></osmChange>)", v)));
  }
}

TEST_CASE("Create node, tag key with > 255 unicode characters", "[osmchange][node][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                           <tag k="{}" v="value"/></node></create></osmChange>)", repeat("😎", 256))),
    http::bad_request, Catch::Message("Key has more than 255 unicode characters in Node -1 at line 2, column 303"));
}


// NODE: INVALID ARGUMENTS, OUT OF RANGE VALUES

TEST_CASE("Modify node, invalid version number", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><modify><node changeset="858" version="a" id="123"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Modify node, version too large", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><modify><node changeset="858" version="999999999999999999999999999999999999" id="123"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Modify node, version negative", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><modify><node changeset="858" version="-1" id="123"/></modify></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, invalid changeset number", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="a"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, changeset number too large", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="999999999999999999999999999999999999" id="-1" lat="1" lon="0"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, changeset number zero", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="0" id="-1" lat="1" lon="0"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, changeset number negative", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="-1" id="-1" lat="1" lon="0"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, longitude not numeric", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="a"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, latitude not numeric", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="858" id="-1" lat="a" lon="0"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, invalid id", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node id="a" changeset="1"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, id too large", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="1" id="999999999999999999999999999999999999" lat="1" lon="0"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create node, id zero", "[osmchange][node][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><node changeset="1" id="0" lat="1" lon="0"/></create></osmChange>)"), http::bad_request);
}



// WAY TESTS

TEST_CASE("Create way, no details", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><way/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create way, only changeset", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><way changeset="123"/></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create way, missing changeset", "[osmchange][way][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><create><way id="-1"/></create></osmChange>)"),
    http::bad_request, Catch::Message("Changeset id is missing for Way -1 at line 1, column 32"));
}

TEST_CASE("Create way, missing node ref", "[osmchange][way][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><create><way changeset="858" id="-1"/></create></osmChange>)"),
    http::precondition_failed, Catch::Message("Precondition failed: Way -1 must have at least one node"));
}

TEST_CASE("Create way, node refs < max way nodes", "[osmchange][way][json]") {
  std::string node_refs{};
  for (uint32_t i = 1; i <= global_settings::get_way_max_nodes(); i++) {
    node_refs += fmt::format(R"(<nd ref="-{}"/>)",  i);
    REQUIRE_NOTHROW(process_testmsg(
      fmt::format(R"(<osmChange><create><way changeset="858" id="-1">{}</way></create></osmChange>)", node_refs)));
  }
}

TEST_CASE("Create way, node refs >= max way nodes", "[osmchange][way][json]") {
  std::string node_refs{};
  for (uint32_t i = 1; i <= global_settings::get_way_max_nodes(); i++)
    node_refs += fmt::format(R"(<nd ref="-{}"/>)", i);
  for (uint32_t j = global_settings::get_way_max_nodes()+1; j < global_settings::get_way_max_nodes() + 10; ++j) {
    node_refs += fmt::format(R"(<nd ref="-{}"/>)", j);
    REQUIRE_THROWS_MATCHES(process_testmsg(
      fmt::format(R"(<osmChange><create><way changeset="858" id="-1">{}</way></create></osmChange>)", node_refs)),
      http::bad_request, Catch::Message(fmt::format("You tried to add {} nodes to way -1, however only {} are allowed", j, global_settings::get_way_max_nodes())));
  }
}

TEST_CASE("Create way, with tags", "[osmchange][way][json]") {
  REQUIRE_NOTHROW(process_testmsg(
    R"(<osmChange><create><way changeset="858" id="-1"><nd ref="-1"/><tag k="key" v="value"/></way></create></osmChange>)"));
}

TEST_CASE("Create way, node ref not numeric", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><way changeset="858" id="-1"><nd ref="a"/><tag k="key" v="value"/></way></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create way, node ref too large", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><way changeset="858" id="-1"><nd ref="999999999999999999999"/><tag k="key" v="value"/></way></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create way, invalid zero node ref", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><way changeset="858" id="-1"><nd ref="0"/><tag k="key" v="value"/></way></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create way, node ref missing", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><way changeset="858" id="-1"><nd ref="1"/><nd /><tag k="key" v="value"/></way></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Delete way, no version", "[osmchange][way][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><delete><way changeset="858" id="-1"/></delete></osmChange>)"),
    http::bad_request);
}

TEST_CASE("Delete way, no id", "[osmchange][way][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><delete><way changeset="858" version="1"/></delete></osmChange>)"),
    http::bad_request, Catch::Message(fmt::format("Mandatory field id missing in object at line 1, column 52")));
}

TEST_CASE("Delete way, no changeset", "[osmchange][way][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><delete><way id="-1" version="1"/></delete></osmChange>)"),
    http::bad_request, Catch::Message(fmt::format("Changeset id is missing for Way -1 at line 1, column 44")));
}

TEST_CASE("Delete way", "[osmchange][way][json]") {
  REQUIRE_NOTHROW(process_testmsg(R"(<osmChange><delete><way changeset="858" id="-1" version="1"/></delete></osmChange>)"));
}


// RELATION TESTS

TEST_CASE("Create relation, id missing", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972"><member type="node" ref="1" role="stop"/></relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, member ref missing", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1"><member type="node" role="stop"/></relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, no member role", "[osmchange][relation][json]") {
  REQUIRE_NOTHROW(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1"><member type="node" ref="-1"/></relation></create></osmChange>)"));
}

TEST_CASE("Create relation, member type missing", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1"><member role="stop" ref="-1"/></relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, invalid member type", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1"><member type="bla" role="stop" ref="-1"/></relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, invalid member ref", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1"><member type="node" ref="a" role="stop"/></relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, invalid member ref zero", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1"><member type="way" ref="0" role="stop"/></relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, member ref too large", "[osmchange][relation][json]") {
  REQUIRE_THROWS_AS(process_testmsg(
    R"(<osmChange><create><relation changeset="972" id="-1">
           <member type="relation" ref="99999999999999999999999999999999" role="stop"/>
           </relation></create></osmChange>)"), http::bad_request);
}

TEST_CASE("Create relation, role with <= 255 unicode characters", "[osmchange][relation][json]") {
  for (int i = 1; i <= 255; i++) {
    auto v = repeat("😎", i);
    REQUIRE_NOTHROW(process_testmsg(
      fmt::format(
               R"(<osmChange><create><relation changeset="858" id="-1">
                           <member type="node" role="{}" ref="123"/>
                  </relation></create></osmChange>)",
           v)));
  }
}

TEST_CASE("Create relation, role with > 255 unicode characters", "[osmchange][relation][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    fmt::format(
               R"(<osmChange><create><relation changeset="858" id="-1">
                           <member type="node" role="{}" ref="123"/>
                  </relation></create></osmChange>)",
           repeat("😎", 256))),
    http::bad_request, Catch::Message("Relation Role has more than 255 unicode characters at line 2, column 321"));
}

TEST_CASE("Delete relation, no version", "[osmchange][relation][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><delete><relation changeset="972" id="-1"/></delete></osmChange>)"),
    http::bad_request, Catch::Message(fmt::format("Version is required when updating Relation -1 at line 1, column 53")));
}

TEST_CASE("Delete relation, no id", "[osmchange][relation][json]") {
  REQUIRE_THROWS_MATCHES(process_testmsg(
    R"(<osmChange><delete><relation changeset="972" version="1"/></delete></osmChange>)"),
    http::bad_request, Catch::Message(fmt::format("Mandatory field id missing in object at line 1, column 57")));
}

TEST_CASE("Delete relation", "[osmchange][relation][json]") {
  REQUIRE_NOTHROW(process_testmsg(
    R"(<osmChange><delete><relation changeset="972" id="123456" version="1"/></delete></osmChange>)"));
}

// INVALID DATA TESTS

TEST_CASE("Invalid data", "[osmchange][json]") {
  REQUIRE_THROWS_AS(process_testmsg("\x3C\x00\x00\x00\x00\x0A\x01\x00"), http::bad_request);
}

*/

// LARGE MESSAGE TESTS

TEST_CASE("Very large JSON message", "[osmchange][node][json]") {

  // Test JSON processing with a very large message
  std::stringstream s;

  s << R"(
      {
        "version": "0.6",
        "generator": "demo",
        "osmChange": [  
     )";

  Test_Parser_Callback cb{};

  for (int i = 1; i < 100000; i++) {

    if (i > 1) {
      s << ",\n";
    }

    api06::Node node;
    node.set_id(-i);
    node.set_changeset(123);
    node.add_tags({{"some key", "some value"}});

    switch (i % 3) {
    case 0:
      node.set_lat(1);
      node.set_lon(2);
      node.set_version(0); // operation create forces version 0, regardless of JSON contents

      cb.nodes.emplace_back(node, operation::op_create, false);

      s << fmt::format(R"(
          {{
            "type": "node",
            "action": "{}",
            "id": {},
            "lat": 1,
            "lon": 2,
            "changeset": 123,
            "tags": {{
              "some key": "some value"
            }}
          }}
         )", "create", -i);

      break;

    case 1:
      node.set_lat(1);
      node.set_lon(2);
      node.set_version(1);

      cb.nodes.emplace_back(node, operation::op_modify, false);

      s << fmt::format(R"(
          {{
            "type": "node",
            "action": "{}",
            "id": {},
            "lat": 1,
            "lon": 2,
            "version": 1,
            "changeset": 123,
            "tags": {{
              "some key": "some value"
            }}
          }}
         )", "modify", -i);
      break;

    case 2:
      node.set_version(1);
      cb.nodes.emplace_back(node, operation::op_delete, false);

      s << fmt::format(R"(
          {{
            "type": "node",
            "action": "{}",
            "id": {},
            "version": 1,
            "changeset": 123,
            "tags": {{
              "some key": "some value"
            }}
          }}
         )", "delete", -i);

      break;

    }
  }

  s << R"(
        ]
      }
    )";

  REQUIRE_NOTHROW(process_testmsg(s.str(), cb));

}

/*

// OBJECT LIMIT TESTS

TEST_CASE("Create node, tags < max tags", "[osmchange][node][json]") {
  auto test_settings = std::unique_ptr<global_settings_test_class>(new global_settings_test_class());
  test_settings->m_element_max_tags = 50;

  global_settings::set_configuration(std::move(test_settings));
  REQUIRE(global_settings::get_element_max_tags());

  std::string tags{};
  for (uint32_t i = 1; i <= global_settings::get_element_max_tags(); i++) {
    tags += fmt::format("<tag k='amenity_{}' v='cafe' />",  i);
    REQUIRE_NOTHROW(process_testmsg(
      fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">{}</node></create></osmChange>)", tags)));
  }
}

TEST_CASE("Create node, tags >= max tags", "[osmchange][node][json]") {
  auto test_settings = std::unique_ptr<global_settings_test_class>(new global_settings_test_class());
  test_settings->m_element_max_tags = 50;

  global_settings::set_configuration(std::move(test_settings));
  REQUIRE(global_settings::get_element_max_tags());

  std::string tags{};
  for (uint32_t i = 1; i <= *global_settings::get_element_max_tags(); i++)
    tags += fmt::format("<tag k='amenity_{}' v='cafe' />",  i);
  for (uint32_t j = *global_settings::get_element_max_tags()+1; j < *global_settings::get_element_max_tags() + 10; ++j) {
    tags += fmt::format("<tag k='amenity_{}' v='cafe' />",  j);
    REQUIRE_THROWS_AS(process_testmsg(
      fmt::format(R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">{}</node></create></osmChange>)", tags)),
      http::bad_request);
  }
}

TEST_CASE("Create relation, members < max members", "[osmchange][relation][json]") {
  auto test_settings = std::unique_ptr<global_settings_test_class>(new global_settings_test_class());
  test_settings->m_relation_max_members = 32000;

  global_settings::set_configuration(std::move(test_settings));
  REQUIRE(global_settings::get_relation_max_members());

  std::string members = repeat(R"(<member type="node" role="demo" ref="123"/>)", *global_settings::get_relation_max_members());
  REQUIRE_NOTHROW(process_testmsg(
    fmt::format(R"(<osmChange><create><relation changeset="858" id="-1">{}"</relation></create></osmChange>)", members)));
}

TEST_CASE("Create relation, members >= max members", "[osmchange][relation][json]") {
  auto test_settings = std::unique_ptr<global_settings_test_class>(new global_settings_test_class());
  test_settings->m_relation_max_members = 32000;

  global_settings::set_configuration(std::move(test_settings));
  REQUIRE(global_settings::get_relation_max_members());

  std::string members = repeat(R"(<member type="node" role="demo" ref="123"/>)", *global_settings::get_relation_max_members());
  for (uint32_t j = *global_settings::get_relation_max_members()+1; j < *global_settings::get_relation_max_members() + 3; ++j) {
    members += R"(<member type="node" role="demo" ref="123"/>)";
    REQUIRE_THROWS_AS(process_testmsg(
      fmt::format(R"(<osmChange><create><relation changeset="858" id="-1">{}"</relation></create></osmChange>)", members)),
      http::bad_request);
  }
}
*/

TEST_CASE("Create node", "[osmchange][node][json]") {

  Test_Parser_Callback cb{};
  api06::Node node;
  node.set_id(-1);
  node.set_lat(42.7957187);
  node.set_lon(13.5690032);
  node.set_changeset(124176968);
  node.set_version(0); // operation create forces version 0, regardless of JSON contents
  node.add_tags({{"man_made", "mast"},{"name", "Monte Piselli - San Giacomo"}});

  cb.nodes.emplace_back(node, operation::op_create, false);

  REQUIRE_NOTHROW(process_testmsg(
    R"(
      {
        "version": "0.6",
        "generator": "demo",
        "osmChange": [
          {
            "type": "node",
            "action": "create",
            "id": -1,
            "lat": 42.7957187,
            "lon": 13.5690032,
            "changeset": 124176968,
            "tags": {
              "man_made": "mast",
              "name": "Monte Piselli - San Giacomo"
            }
          }
        ]
      }
    )", cb));
}

TEST_CASE("Create way", "[osmchange][way][json]") {

  Test_Parser_Callback cb{};
  api06::Way way;
  way.set_id(-1);
  way.set_changeset(124176968);
  way.set_version(0); // operation create forces version 0, regardless of JSON contents
  way.add_way_nodes({1,2,3,4});
  way.add_tags({{"highway", "residential"},{"name", "Via Monte"}});

  cb.ways.emplace_back(way, operation::op_create, false);

  REQUIRE_NOTHROW(process_testmsg(
    R"(
      {
        "version": "0.6",
        "generator": "demo",
        "osmChange": [
          {
            "type": "way",
            "action": "create",
            "id": -1,
            "changeset": 124176968,
            "nodes": [1,2,3,4],
            "tags": {
              "highway": "residential",
              "name": "Via Monte"
            }
          }
        ]
      }
    )", cb));
}

TEST_CASE("Create relation", "[osmchange][relation][json]") {

  Test_Parser_Callback cb{};
  api06::Relation rel;
  rel.set_id(-1);
  rel.set_changeset(124176968);
  rel.set_version(0); // operation create forces version 0, regardless of JSON contents
  rel.add_tags({{"route", "bus"}, {"ref", "23"}});  // last ref tag wins
  rel.add_members({{"Node", -1, "stop"}, {"Way", -2, ""}, {"Relation", -3, "parent"}});

  cb.relations.emplace_back(rel, operation::op_create, false);

  REQUIRE_NOTHROW(process_testmsg(
    R"(
      {
        "version": "0.6",
        "generator": "demo",
        "osmChange": [
          {
            "type": "relation",
            "action": "create",
            "id": -1,
            "changeset": 124176968,
            "members": [
                          {"type": "Node", "ref": -1, "role": "stop"},
                          {"type": "Way", "ref": -2},
                          {"type": "Relation", "ref": -3, "role": "parent"}
                       ],
            "tags": {
              "ref": "123",
              "route": "bus",
              "ref": "23"
            }
          }
        ]
      }
    )", cb));
}
