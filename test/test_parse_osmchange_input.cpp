
#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"
#include "cgimap/api06/changeset_upload/parser_callback.hpp"

#include <iostream>
#include <stdexcept>

class Test_Parser_Callback : public Parser_Callback {

public:
  Test_Parser_Callback() : start_executed(false), end_executed(false) {}

  void start_document() { start_executed = true; }

  void end_document() { end_executed = true; }

  void process_node(const Node &, operation op, bool if_unused) {}

  void process_way(const Way &, operation op, bool if_unused) {}

  void process_relation(const Relation &, operation op, bool if_unused) {}

  bool start_executed;
  bool end_executed;
};

void process_testmsg(const std::string &payload) {

  Test_Parser_Callback cb;
  OSMChangeXMLParser parser(&cb);
  parser.process_message(payload);
}

void test_osmchange_structure() {

  // Invalid XML
  try {
    process_testmsg(R"(<osmChange>)");
    throw std::runtime_error("test_nodes::001: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400) {
      std::runtime_error("test_osmchange_structure::001: Expected HTTP/400");
    }
  }

  //  XML without any changes
  try {
    process_testmsg(R"(<osmChange/>)");
  } catch (http::exception &e) {
    throw std::runtime_error("test_osmchange_structure::002: Unexpected Exception");
  }

  //  XML without any changes
  try {
    process_testmsg(R"(<osmChange2/>)");
    throw std::runtime_error("test_osmchange_structure::003: Expected exception");
  } catch (http::exception &e) {
  }

  // Unknown action
  try {
    process_testmsg(R"(<osmChange><dummy/></osmChange>)");
    throw std::runtime_error("test_osmchange_structure::004: Expected exception");
  } catch (http::exception &e) {
      if (e.code() != 400)
	throw std::runtime_error("test_osmchange_structure::004: Expected HTTP/400");
      if (std::string(e.what()) != "Unknown action dummy, choices are create, modify, delete")
	throw std::runtime_error("test_osmchange_structure::004: Expected unknown action error");
  }

  // Create action
  try {
    process_testmsg(R"(<osmChange><create/></osmChange>)");
  } catch (http::exception &e) {
      throw std::runtime_error("test_osmchange_structure::005: Unexpected Exception");
  }

  // Modify action
  try {
    process_testmsg(R"(<osmChange><modify/></osmChange>)");
  } catch (http::exception &e) {
      throw std::runtime_error("test_osmchange_structure::006: Unexpected Exception");
  }

  // Delete action
  try {
    process_testmsg(R"(<osmChange><delete/></osmChange>)");
  } catch (http::exception &e) {
      throw std::runtime_error("test_osmchange_structure::007: Unexpected Exception");
  }
}


void test_node() {


}

void test_way() {


}

void test_relation() {


}


int main(int argc, char *argv[]) {
  try {
      test_osmchange_structure();
      test_node();
      test_way();
      test_relation();

  } catch (const std::exception &ex) {
    std::cerr << "EXCEPTION: " << ex.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN ERROR" << std::endl;
    return 1;
  }

  return 0;
}
