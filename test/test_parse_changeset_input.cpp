#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"


#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

  std::string repeat(const std::string &input, size_t num) {
    std::ostringstream os;
    std::fill_n(std::ostream_iterator<std::string>(os), num, input);
    return os.str();
  }


std::map<std::string, std::string> process_testmsg(const std::string &payload) {


  api06::ChangesetXMLParser parser{};
  return parser.process_message(payload);
}

void test_changeset_xml() {

  // Invalid XML
  try {
    process_testmsg(R"(<osm>)");
    throw std::runtime_error(
        "test_changeset_xml::001: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400) {
      std::runtime_error("test_changeset_xml::001: Expected HTTP 400");
    }
  }

  // Invalid XML
  try {
    process_testmsg(R"(bla)");
    throw std::runtime_error(
        "test_changeset_xml::002: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400) {
      std::runtime_error("test_changeset_xml::002: Expected HTTP 400");
    }
  }

  //  Missing changeset tag
  try {
    process_testmsg(R"(<osm/>)");
    throw std::runtime_error(
        "test_changeset_xml::003: Expected exception");
  } catch (http::exception &e) {
      if (e.code() != 400) {
        std::runtime_error("test_changeset_xml::003: Expected HTTP 400");
      }
  }

  // Changeset without tags
  try {
    auto tags = process_testmsg(R"(<osm><changeset/></osm>)");
    if (!tags.empty())
      throw std::runtime_error(
          "test_osmchange_structure::004: Expected empty tags");

  } catch (http::exception &e) {
      throw std::runtime_error(
          "test_osmchange_structure::004: Unexpected Exception");
  }

  // Changeset with multiple identical tags
  try {
    auto tags = process_testmsg(R"(<osm>
                                       <changeset>
                                            <tag k="key1" v="val1"/>
                                            <tag k="key2" v="val2"/>
                                            <tag k="key1" v="val1NEW"/>
                                        </changeset>
                                   </osm>)");
    if (tags.size() != 2)
      throw std::runtime_error(
          "test_osmchange_structure::005 Expected empty tags");

    if (tags["key1"] != "val1NEW") {
	      throw std::runtime_error(
	          "test_osmchange_structure::005 Incorrect key1");
    }

  } catch (http::exception &e) {
      throw std::runtime_error(
          "test_osmchange_structure::005: Unexpected Exception");
  }

  // Changeset with multiple changeset tags and identical tags
  try {
    auto tags = process_testmsg(R"(<osm>
                                       <changeset>
                                            <tag k="key1" v="val1"/>
                                            <tag k="key2" v="val2"/>
                                       </changeset>
                                       <changeset>
                                            <tag k="key1" v="val1NEW"/>
                                        </changeset>
                                   </osm>)");
    if (tags.size() != 2)
      throw std::runtime_error(
          "test_osmchange_structure::005 Expected empty tags");

    if (tags["key1"] != "val1NEW") {
	      throw std::runtime_error(
	          "test_osmchange_structure::005 Incorrect key1");
    }

  } catch (http::exception &e) {
      throw std::runtime_error(
          "test_osmchange_structure::005: Unexpected Exception");
  }


  // Tag: Key without value
  try {
    process_testmsg(
        R"(<osm><changeset><tag k="key"/></changeset></osm>)");
    throw std::runtime_error("test_osmchange_structure::005: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_osmchange_structure::005: Expected HTTP 400");
  }

  // Tag: Value without key
  try {
    process_testmsg(
        R"(<osm><changeset><tag v="value"/></changeset></osm>)");
    throw std::runtime_error("test_osmchange_structure::006: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_osmchange_structure::006: Expected HTTP 400");
  }

  // Tag: Value with max 255 unicode characters
  for (int i = 0; i <= 256; i++) {
    auto v = repeat("😎", i);

    try {
      process_testmsg(
          fmt::format(
               R"(<osm><changeset><tag k="key" v="{}"/></changeset></osm>)",
           v));
      if (i > 255)
        throw std::runtime_error("test_osmchange_structure::010: Expected exception for "
                                 "string length > 255 unicode characters");
    } catch (http::exception &e) {
      if (e.code() != 400)
        throw std::runtime_error("ttest_osmchange_structure::010: Expected HTTP 400");
      const std::string expected = "Value has more than 255 unicode characters";
      const std::string actual = std::string(e.what()).substr(0, expected.size());
      if (actual != expected)
        throw std::runtime_error("test_osmchange_structure::010: Expected Value has more than "
                                 "255 unicode characters, got: " + actual);
      if (i <= 255)
        throw std::runtime_error("test_osmchange_structure::010: Unexpected exception for "
                                 "string length <= 255 characters");
    }
  }

  // Tag: Key with max 255 unicode characters
  for (int i = 1; i <= 256; i++) {
    auto v = repeat("😎", i);

    try {
      process_testmsg(
          fmt::format(
               R"(<osm><changeset><tag k="{}" v="value"/></changeset></osm>)",
           v));
      if (i > 255)
        throw std::runtime_error("test_osmchange_structure::011: Expected exception for "
                                 "string length > 255 unicode characters");
    } catch (http::exception &e) {
      if (e.code() != 400)
        throw std::runtime_error("test_osmchange_structure::011: Expected HTTP 400");
      const std::string expected = "Key has more than 255 unicode characters";
      const std::string actual = std::string(e.what()).substr(0, expected.size());
      if (actual != expected)
        throw std::runtime_error("test_osmchange_structure::011: Expected " + expected + ", got: " + actual);
      if (i <= 255)
        throw std::runtime_error("test_osmchange_structure::011: Unexpected exception for "
                                 "string length <= 255 characters");
    }
  }



}

}

int main(int argc, char *argv[]) {
  try {
      test_changeset_xml();

  } catch (const std::exception &ex) {
    std::cerr << "EXCEPTION: " << ex.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN ERROR" << std::endl;
    return 1;
  }

  return 0;
}
