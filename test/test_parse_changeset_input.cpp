#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"


#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {


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
