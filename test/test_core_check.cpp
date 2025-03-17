/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "test_core_helper.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <filesystem>
#include <set>
#include <sstream>

struct ExceptionSubstringMatcher : Catch::MatcherBase<std::runtime_error> {
  std::string m_text;

  ExceptionSubstringMatcher(char const *text) : m_text(text) {}

  bool match(std::runtime_error const &arg) const override {
    return std::string(arg.what()).find(m_text) != std::string::npos;
  }

  std::string describe() const override {
    return "I'm expecting to find the following text in the exception: " + m_text;
  }
};

// *****************************************************************************
// NOTE: don't reformat payload_expected* and payload_actual* strings,
//       they are used to test the check_response function. Leading spaces
//       are not permitted and would break the tests.
// *****************************************************************************

// Test payload 1

std::string payload_expected_1 =
R"(Content-Type: application/xml; charset=utf-8
!Content-Disposition:
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

std::string payload_actual_1 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

// Test payload 2

std::string payload_expected_2 =
R"(Content-Type: application/xml; charset=utf-8
!Content-Disposition:
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

std::string payload_actual_2 =
R"(Content-Type: application/xml; charset=utf-8
Content-Disposition: attachment; filename="invalid_header.exe"
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

// Test payload 3

std::string payload_expected_3 =
R"(Content-Type: application/xml; charset=utf-8
Content-Disposition: attachment; filename="map.osm"
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

std::string payload_actual_3 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

// Test payload 4

std::string payload_expected_4 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

std::string payload_actual_4 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";


TEST_CASE("check_response HTTP header validation", "[test_core_check]") {


  SECTION("Extra HTTP header in response") {
    std::istringstream expected(payload_expected_2);
    std::istringstream actual(payload_actual_2);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
       ExceptionSubstringMatcher("ERROR: Expected not to find header `Content-Disposition', but it is present."));
  }

  SECTION("Missing Content-Disposition HTTP header in response") {
    std::istringstream expected(payload_expected_3);
    std::istringstream actual(payload_actual_3);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
       ExceptionSubstringMatcher("ERROR: Expected header `Content-Disposition: attachment; filename=\"map.osm\"', but didn't find it in actual response."));
  }

  SECTION("Deviating value for HTTP header in response") {
    std::istringstream expected(payload_expected_4);
    std::istringstream actual(payload_actual_4);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("ERROR: Header key `Content-Type'; expected `application/xml; charset=utf-8' but got `application/json; charset=utf-8'."));
  }
}


// Test payload 10 - Missing node element
std::string payload_expected_10 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0" />
</osm>
)";

std::string payload_actual_10 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
</osm>
)";

// Test payload 11 - Extra node element
std::string payload_expected_11 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0" />
</osm>
)";

std::string payload_actual_11 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0" />
  <node id="2" lat="1.0" lon="1.0" />
</osm>
)";

// Test payload 12 - Missing required attribute in node
std::string payload_expected_12 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0" />
</osm>
)";

std::string payload_actual_12 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" />
</osm>
)";

// Test payload 13 - Incorrect XML format
std::string payload_expected_13 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0" />
</osm>
)";

std::string payload_actual_13 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0"
</osm>
)";


// Test payload 14 - Different tag values
std::string payload_expected_14 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0">
    <tag k="foo1" v="bar1"/>
    <tag k="highway" v="motorway"/>
  </node>
</osm>
)";

std::string payload_actual_14 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0">
    <tag k="foo1" v="bar2"/>
    <tag k="highway" v="motorway"/>
  </node>
</osm>
)";


// Test payload 15 - Missing attribute
std::string payload_expected_15 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK
---
<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0">
    <tag k="foo1" v="bar1"/>
    <tag k="highway" v="motorway"/>
  </node>
</osm>
)";

std::string payload_actual_15 =
R"(Content-Type: application/xml; charset=utf-8
Status: 200 OK

<osm version="0.6" generator="***" copyright="***" attribution="***" license="***">
  <node id="1" lat="0.0" lon="0.0">
    <tag k="foo1" v="bar1"/>
    <tag k="highway"/>
  </node>
</osm>
)";




TEST_CASE("check_response HTTP XML body validation", "[test_core_check]") {

  SECTION("Identical XML responses") {
    std::istringstream expected(payload_expected_1);
    std::istringstream actual(payload_actual_1);
    REQUIRE_NOTHROW(check_response(expected, actual));
  }

  SECTION("Missing node element in actual payload") {
    std::istringstream expected(payload_expected_10);
    std::istringstream actual(payload_actual_10);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Actual result has fewer entries than expected: [node] are absent"));
  }

  SECTION("Extra node element in actual payload") {
    std::istringstream expected(payload_expected_11);
    std::istringstream actual(payload_actual_11);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Actual result has more entries than expected: [node] are extra"));
  }

  SECTION("Missing required attribute in node element") {
    std::istringstream expected(payload_expected_12);
    std::istringstream actual(payload_actual_12);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Attributes differ: [lon], in \"<xmlattr>\" element, in \"node\" element"));
  }

  SECTION("Incorrect XML format in actual payload") {
    std::istringstream expected(payload_expected_13);
    std::istringstream actual(payload_actual_13);
    REQUIRE_THROWS_AS(check_response(expected, actual), std::runtime_error);
  }

  SECTION("Different tag values") {
    std::istringstream expected(payload_expected_14);
    std::istringstream actual(payload_actual_14);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Attribute `v' expected value `bar1', but got `bar2'"));
  }

  SECTION("Mising tag value attribute") {
    std::istringstream expected(payload_expected_15);
    std::istringstream actual(payload_actual_15);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Attributes differ: [v]"));
  }
}

// Test payload 20 - Missing node element
std::string payload_expected_20 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK
---
{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      }
  ]
}
)";

std::string payload_actual_20 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [

  ]
}
)";

// Test payload 21 - Extra node element
std::string payload_expected_21 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK
---
{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      }
  ]
}
)";

std::string payload_actual_21 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      },
      { "type": "node",
        "id": 2,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      }
  ]
}
)";

// Test payload 22 - Missing required attribute in node
std::string payload_expected_22 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK
---
{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      }
  ]
}
)";

std::string payload_actual_22 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      }
  ]
}
)";

// Test payload 23 - Incorrect XML format
std::string payload_expected_23 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK
---
{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1
      }
  ]
}
)";

std::string payload_actual_23 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
      { "type": "node",
        "id": 1,
	"lat": 0.0000000,
	"lon": 0.0000000,
	"timestamp": "2012-09-25T00:00:00Z",
	"version": 1,
	"changeset": 1,
	"user": "foo",
	"uid": 1

  ]
}
)";


// Test payload 24 - Different tag values
std::string payload_expected_24 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK
---
{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
    {
      "type": "node",
      "id": 40053,
      "lat": 0.9965753,
      "lon": 1.1558749,
      "timestamp": "2012-09-25T00:00:03Z",
      "version": 1,
      "changeset": 1,
      "user": "foo",
      "uid": 1,
      "tags": {
        "board_type": "history",
        "information": "board",
        "tourism": "information"
      }
    }
  ]
}
)";

std::string payload_actual_24 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
    {
      "type": "node",
      "id": 40053,
      "lat": 0.9965753,
      "lon": 1.1558749,
      "timestamp": "2012-09-25T00:00:03Z",
      "version": 1,
      "changeset": 1,
      "user": "foo",
      "uid": 1,
      "tags": {
        "board_type": "history2",
        "information": "board",
        "tourism": "information"
      }
  }
  ]
}
)";


// Test payload 25 - Different tag sequence
std::string payload_expected_25 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK
---
{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
    {
      "type": "node",
      "id": 40053,
      "lat": 0.9965753,
      "lon": 1.1558749,
      "timestamp": "2012-09-25T00:00:03Z",
      "version": 1,
      "changeset": 1,
      "user": "foo",
      "uid": 1,
      "tags": {
        "board_type": "history",
        "information": "board",
        "tourism": "information"
      }
    }
  ]
}
)";

std::string payload_actual_25 =
R"(Content-Type: application/json; charset=utf-8
Status: 200 OK

{ "version": "0.6",
  "generator": "***",
  "copyright": "***",
  "attribution": "***",
  "license": "***",
  "elements": [
    {
      "type": "node",
      "id": 40053,
      "lat": 0.9965753,
      "lon": 1.1558749,
      "timestamp": "2012-09-25T00:00:03Z",
      "version": 1,
      "changeset": 1,
      "user": "foo",
      "uid": 1,
      "tags": {
        "tourism": "information",
        "board_type": "history",
        "information": "board"
      }
    }
  ]
}
)";


TEST_CASE("check_response HTTP JSON body validation", "[test_core_check]") {

  SECTION("Missing node element in actual payload") {
    std::istringstream expected(payload_expected_20);
    std::istringstream actual(payload_actual_20);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Actual result has fewer entries than expected"));
  }

  SECTION("Extra node element in actual payload") {
    std::istringstream expected(payload_expected_21);
    std::istringstream actual(payload_actual_21);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Actual result has more entries than expected"));
  }

  SECTION("Missing required attribute in node element") {
    std::istringstream expected(payload_expected_22);
    std::istringstream actual(payload_actual_22);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Expected lon, but got timestamp"));
  }

  SECTION("Incorrect JSON format in actual payload") {
    std::istringstream expected(payload_expected_23);
    std::istringstream actual(payload_actual_23);
    REQUIRE_THROWS_AS(check_response(expected, actual), std::runtime_error);
  }

  SECTION("Different tag values") {
    std::istringstream expected(payload_expected_24);
    std::istringstream actual(payload_actual_24);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Expected 'history', but got 'history2', in \"board_type\" element"));
  }

  SECTION("Different tag sequence") {
    std::istringstream expected(payload_expected_25);
    std::istringstream actual(payload_actual_25);
    REQUIRE_THROWS_MATCHES(check_response(expected, actual), std::runtime_error,
      ExceptionSubstringMatcher("Expected board_type, but got tourism"));
  }
}

