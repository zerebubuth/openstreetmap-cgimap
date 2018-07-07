
#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"
#include "cgimap/api06/changeset_upload/parser_callback.hpp"

#include <iostream>
#include <sstream>
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

std::string repeat(const std::string &input, size_t num) {
  std::ostringstream os;
  std::fill_n(std::ostream_iterator<std::string>(os), num, input);
  return os.str();
}

void process_testmsg(const std::string &payload) {

  Test_Parser_Callback cb;
  OSMChangeXMLParser parser(&cb);
  parser.process_message(payload);
}

void test_osmchange_structure() {

  // Invalid XML
  try {
    process_testmsg(R"(<osmChange>)");
    throw std::runtime_error(
        "test_osmchange_structure::001: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400) {
      std::runtime_error("test_osmchange_structure::001: Expected HTTP/400");
    }
  }

  // Invalid XML
  try {
    process_testmsg(R"(bla)");
    throw std::runtime_error(
        "test_osmchange_structure::002: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400) {
      std::runtime_error("test_osmchange_structure::002: Expected HTTP/400");
    }
  }

  //  XML without any changes
  try {
    process_testmsg(R"(<osmChange/>)");
  } catch (http::exception &e) {
    throw std::runtime_error(
        "test_osmchange_structure::003: Unexpected Exception");
  }

  //  XML without any changes
  try {
    process_testmsg(R"(<osmChange2/>)");
    throw std::runtime_error(
        "test_osmchange_structure::004: Expected exception");
  } catch (http::exception &e) {
  }

  // Unknown action
  try {
    process_testmsg(R"(<osmChange><dummy/></osmChange>)");
    throw std::runtime_error(
        "test_osmchange_structure::005: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error(
          "test_osmchange_structure::005: Expected HTTP/400");
    if (std::string(e.what()) !=
        "Unknown action dummy, choices are create, modify, delete")
      throw std::runtime_error(
          "test_osmchange_structure::005: Expected unknown action error");
  }

  // Create action
  try {
    process_testmsg(R"(<osmChange><create/></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error(
        "test_osmchange_structure::006: Unexpected Exception");
  }

  // Modify action
  try {
    process_testmsg(R"(<osmChange><modify/></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error(
        "test_osmchange_structure::007: Unexpected Exception");
  }

  // Delete action
  try {
    process_testmsg(R"(<osmChange><delete/></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error(
        "test_osmchange_structure::008: Unexpected Exception");
  }

  // Invalid object
  try {
    process_testmsg(R"(<osmChange><create><bla/></create></osmChange>)");
    throw std::runtime_error(
        "test_osmchange_structure::009: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error(
          "test_osmchange_structure::009: Expected HTTP/400");
    if (std::string(e.what()) !=
        "Unknown element bla, expecting node, way or relation")
      throw std::runtime_error(
          "test_osmchange_structure::009: Expected unknown action error");
  }
}

void test_node() {

  // All details for node missing
  try {
    process_testmsg(R"(<osmChange><create><node/></create></osmChange>)");
    throw std::runtime_error("test_node::001: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::001: Expected HTTP/400");
    //      if (std::string(e.what()) != "Changeset id is missing for Node/0")
    //	throw std::runtime_error("test_node::001: Expected unknown action
    // error");
  }

  // All details except changeset missing
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="123"/></create></osmChange>)");
    throw std::runtime_error("test_node::002: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::002: Expected HTTP/400");
  }

  // Both latitude and longitude missing
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="123" id="-1"/></create></osmChange>)");
    throw std::runtime_error("test_node::003: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::003: Expected HTTP/400");
  }

  // Latitude missing
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lon="2"/></create></osmChange>)");
    throw std::runtime_error("test_node::004: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::004: Expected HTTP/400");
  }

  // Longitude missing
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="2"/></create></osmChange>)");
    throw std::runtime_error("test_node::005: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::005: Expected HTTP/400");
  }

  // Latitude outside valid range
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="90.01" lon="2"/></create></osmChange>)");
    throw std::runtime_error("test_node::006: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::006: Expected HTTP/400");
  }

  // Latitude outside valid range
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-90.01" lon="2"/></create></osmChange>)");
    throw std::runtime_error("test_node::007: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::007: Expected HTTP/400");
  }

  // Latitude outside valid range
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-90.01" lon="2"/></create></osmChange>)");
    throw std::runtime_error("test_node::008: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::008: Expected HTTP/400");
  }

  // Longitude outside valid range
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="180.01"/></create></osmChange>)");
    throw std::runtime_error("test_node::009: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::009: Expected HTTP/400");
  }

  // Longitude outside valid range
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="-180.01"/></create></osmChange>)");
    throw std::runtime_error("test_node::010: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::010: Expected HTTP/400");
  }

  // Changeset missing
  try {
    process_testmsg(
        R"(<osmChange><create><node id="-1" lat="-90.00" lon="-180.00"/></create></osmChange>)");
    throw std::runtime_error("test_node::011: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::011: Expected HTTP/400");
    if (std::string(e.what()) != "Changeset id is missing for Node/-1")
      throw std::runtime_error(
          "test_node::018: Expected Changeset id is missing for Node/-1");
  }

  // Redefined lat attribute
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-90.00" lon="-180.00" lat="20"/></create></osmChange>)");
    throw std::runtime_error("test_node::012: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::012: Expected HTTP/400");
  }

  // Valid create node
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="180.00"/></create></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error("test_node::020: Unexpected Exception");
  }

  // Valid create node
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-90.00" lon="-180.00"/></create></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error("test_node::021: Unexpected Exception");
  }

  // Modify node without version
  try {
    process_testmsg(
        R"(<osmChange><modify><node changeset="858" id="123" lat="90.00" lon="180.00"/></modify></osmChange>)");
    throw std::runtime_error("test_node::022: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::022: Expected HTTP/400");
  }

  // Modify node with invalid version
  try {
    process_testmsg(
        R"(<osmChange><modify><node changeset="858" version="0" id="123"/></modify></osmChange>)");
    throw std::runtime_error("test_node::023: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::023: Expected HTTP/400");
  }

  // Delete node without version
  try {
    process_testmsg(
        R"(<osmChange><delete><node changeset="858" id="123"/></delete></osmChange>)");
    throw std::runtime_error("test_node::024: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::024: Expected HTTP/400");
  }

  // Delete node without id
  try {
    process_testmsg(
        R"(<osmChange><delete><node changeset="858" version="1"/></modify></osmChange>)");
    throw std::runtime_error("test_node::025: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::025: Expected HTTP/400");
  }

  // Delete node with invalid version
  try {
    process_testmsg(
        R"(<osmChange><delete><node changeset="858" version="0" id="123"/></modify></osmChange>)");
    throw std::runtime_error("test_node::026: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::026: Expected HTTP/400");
  }

  // xml message nested too deep
  try {
    process_testmsg(
        R"( <osmChange><create><node changeset="858" id="-1" lat="-90.00" lon="-180.00">
                          <tag k="1" v="2"><blubb/></tag></node></create></osmChange>)");
    throw std::runtime_error("test_node::027: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::027: Expected HTTP/400");
  }

  // Key may not be empty
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag k="" v="value"/></node></create></osmChange>)");
    throw std::runtime_error("test_node::030: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::030: Expected HTTP/400");
  }

  // Duplicate key dup1
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag k="key1" v="value1"/>
                       <tag k="dup1" v="value2"/>
                       <tag k="dup1" v="value3"/>
                       <tag k="key3" v="value4"/>
                       </node></create></osmChange>)");
    throw std::runtime_error("test_node::031: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::031: Expected HTTP/400");
    if (std::string(e.what()) !=
        "Element Node/-1 has duplicate tags with key dup1")
      throw std::runtime_error(
          "test_node::018: Expected has duplicate tags with key dup1");
  }

  // Tag: Key without value
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag k="key"/></node></create></osmChange>)");
    throw std::runtime_error("test_node::032: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::032: Expected HTTP/400");
  }

  // Tag: Value without key
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                       <tag v="value"/></node></create></osmChange>)");
    throw std::runtime_error("test_node::033: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::033: Expected HTTP/400");
  }

  // Tag: Value with max 255 unicode characters
  for (int i = 0; i <= 256; i++) {
    auto v = repeat("😎", i);

    try {
      process_testmsg(
          (boost::format(
               R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                           <tag k="key" v="%1%"/></node></create></osmChange>)") %
           v).str());
      if (i > 255)
        throw std::runtime_error("test_node::040: Expected exception for "
                                 "string length > 255 unicode characters");
    } catch (http::exception &e) {
      if (e.code() != 400)
        throw std::runtime_error("test_node::040: Expected HTTP/400");
      if (std::string(e.what()) !=
          "Value has more than 255 unicode characters in object Node/-1")
        throw std::runtime_error("test_node::040: Expected Value has more than "
                                 "255 unicode characters");
      if (i <= 255)
        throw std::runtime_error("test_node::040: Unexpected exception for "
                                 "string length <= 255 characters");
    }
  }

  // Tag: Key with max 255 unicode characters
  for (int i = 1; i <= 256; i++) {
    auto v = repeat("😎", i);

    try {
      process_testmsg(
          (boost::format(
               R"(<osmChange><create><node changeset="858" id="-1" lat="-1" lon="2">
                           <tag k="%1%" v="value"/></node></create></osmChange>)") %
           v).str());
      if (i > 255)
        throw std::runtime_error("test_node::041: Expected exception for "
                                 "string length > 255 unicode characters");
    } catch (http::exception &e) {
      if (e.code() != 400)
        throw std::runtime_error("test_node::041: Expected HTTP/400");
      if (std::string(e.what()) !=
          "Key has more than 255 unicode characters in object Node/-1")
        throw std::runtime_error("test_node::041: Expected Key has more than "
                                 "255 unicode characters");
      if (i <= 255)
        throw std::runtime_error("test_node::041: Unexpected exception for "
                                 "string length <= 255 characters");
    }
  }

  // Tag: Value with ampersand character: libxml2 parser needs XML_PARSE_NOENT option so that the &amp;
  // character doesn't get replaced by &#38; but a proper & character. Otherwise, the string will
  // exceed the 255 unicode character check, and an exception would be raised.
  try {
    process_testmsg(R"(
     <osmChange version="0.6" generator="JOSM">
     <create>
       <node id='-39094' changeset='1135' lat='40.72184689864' lon='-73.99968913726'>
         <tag k='amenity' v='cafe' />
         <tag k='cuisine' v='coffee_shop' />
         <tag k='description' v='&quot;Project Cozy is the latest addition to Nolita serving La Colombe coffee, specialty drinks like the Cozy Mint Coffee and Charcoal Latte, fresh &amp; made to order juices and smoothies, sandwiches, and pastries by Bibble &amp; Sip, a renowned bakery in Midtown&quot;' />
       </node>
     </create>
     </osmChange>
    )");
  } catch (http::exception &e) {
    throw std::runtime_error(
        "test_node::042: Unexpected Exception");
  }

  // INVALID ARGUMENTS, OUT OF RANGE VALUES


  // Invalid version number
  try {
    process_testmsg(
        R"(<osmChange><modify><node changeset="858" version="a" id="123"/></modify></osmChange>)");
    throw std::runtime_error("test_node::060: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::060: Expected HTTP/400");
  }

  // Version too large
  try {
    process_testmsg(
        R"(<osmChange><modify><node changeset="858" version="999999999999999999999999999999999999" id="123"/></modify></osmChange>)");
    throw std::runtime_error("test_node::061: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::061: Expected HTTP/400");
  }

  // Version negative
  try {
    process_testmsg(
        R"(<osmChange><modify><node changeset="858" version="-1" id="123"/></modify></osmChange>)");
    throw std::runtime_error("test_node::062: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::062: Expected HTTP/400");
  }

  // Invalid changeset number
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="a"/></create></osmChange>)");
    throw std::runtime_error("test_node::070: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::070: Expected HTTP/400");
  }


  // Changeset too large
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="999999999999999999999999999999999999" id="-1" lat="1" lon="0"/></create></osmChange>)");
    throw std::runtime_error("test_node::071: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::071: Expected HTTP/400");
  }

  // Changeset is zero
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="0" id="-1" lat="1" lon="0"/></create></osmChange>)");
    throw std::runtime_error("test_node::072: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::072: Expected HTTP/400");
  }

  // Changeset not positive
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="-1" id="-1" lat="1" lon="0"/></create></osmChange>)");
    throw std::runtime_error("test_node::073: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::073: Expected HTTP/400");
  }

  // Longitude not numeric
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="90.00" lon="a"/></create></osmChange>)");
    throw std::runtime_error("test_node::080: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::080: Expected HTTP/400");
  }

  // Latitude not numeric
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="858" id="-1" lat="a" lon="0"/></create></osmChange>)");
    throw std::runtime_error("test_node::090: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::090: Expected HTTP/400");
  }

  // Invalid Id number
  try {
    process_testmsg(
        R"(<osmChange><create><node id="a" changeset="1"/></create></osmChange>)");
    throw std::runtime_error("test_node::100: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::100: Expected HTTP/400");
  }

  // Id too large
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="1" id="999999999999999999999999999999999999" lat="1" lon="0"/></create></osmChange>)");
    throw std::runtime_error("test_node::101: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::101: Expected HTTP/400");
  }

  // Id not zero
  try {
    process_testmsg(
        R"(<osmChange><create><node changeset="1" id="0" lat="1" lon="0"/></create></osmChange>)");
    throw std::runtime_error("test_node::102: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_node::102: Expected HTTP/400");
  }

}

void test_way() {

  // All details for way missing
  try {
    process_testmsg(R"(<osmChange><create><way/></create></osmChange>)");
    throw std::runtime_error("test_way::001: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_way::001: Expected HTTP/400");
    //      if (std::string(e.what()) != "Changeset id is missing for Node/0")
    //	throw std::runtime_error("test_node::001: Expected unknown action
    // error");
  }

  // All details except changeset missing
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="123"/></create></osmChange>)");
    throw std::runtime_error("test_way::002: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_way::002: Expected HTTP/400");
  }

  // Changeset missing
  try {
    process_testmsg(
        R"(<osmChange><create><way id="-1"/></create></osmChange>)");
    throw std::runtime_error("test_way::003: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_way::003: Expected HTTP/400");
    if (std::string(e.what()) != "Changeset id is missing for Way/-1")
      throw std::runtime_error(
          "test_way::003: Expected Changeset id is missing for Way/-1");
  }

  // Node ref missing
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="858" id="-1"/></create></osmChange>)");
    throw std::runtime_error("test_way::010: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 412)
      throw std::runtime_error("test_way::010: Expected HTTP/412");
    if (std::string(e.what()) != "Way -1 must have at least one node")
      throw std::runtime_error(
          "test_way::010: Expected Way -1 must have at least one node");
  }

  // Test node refs up to max number of nodes per way
  {
    std::ostringstream os;

    for (int i = 1; i <= WAY_MAX_NODES + 1; i++) {
      os << (boost::format(R"(<nd ref="-%1%"/>)") % i).str();

      try {
        process_testmsg(
            (boost::format(
                 R"(<osmChange><create><way changeset="858" id="-1">%1%</way></create></osmChange>)") %
             os.str())
                .str());
        if (i > WAY_MAX_NODES)
          throw std::runtime_error(
              "test_way::020: Expected exception for way max nodes exceeded");
      } catch (http::exception &e) {
        if (e.code() != 400)
          throw std::runtime_error("test_way::020: Expected HTTP/400");
        if (std::string(e.what()) !=
            (boost::format(
                 "You tried to add %1% nodes to way %2%, however only "
                 "%3% are allowed") %
             i % -1 % WAY_MAX_NODES)
                .str())
          throw std::runtime_error(
              "test_node::040: Expected: you tried to add x nodes to way");
        if (i <= WAY_MAX_NODES)
          throw std::runtime_error(
              "test_way::020: Unexpected exception for way "
              "max nodes below limit");
      }
    }
  }

  // Way with tags
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="858" id="-1"><nd ref="-1"/><tag k="key" v="value"/></way></create></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error("test_way::025: Unexpected exception");
  }

  // ref not numeric
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="858" id="-1"><nd ref="a"/><tag k="key" v="value"/></way></create></osmChange>)");
    throw std::runtime_error("test_way::030: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_way::030: Expected HTTP/400");
  }

  // ref too large
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="858" id="-1"><nd ref="999999999999999999999"/><tag k="key" v="value"/></way></create></osmChange>)");
    throw std::runtime_error("test_way::031: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_way::031: Expected HTTP/400");
  }

  // Invalid zero ref
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="858" id="-1"><nd ref="0"/><tag k="key" v="value"/></way></create></osmChange>)");
    throw std::runtime_error("test_way::032: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_way::032: Expected HTTP/400");
  }

  // Ref missing
  try {
    process_testmsg(
        R"(<osmChange><create><way changeset="858" id="-1"><nd ref="1"/><nd /><tag k="key" v="value"/></way></create></osmChange>)");
    throw std::runtime_error("test_way::033: Expected exception");
  } catch (http::exception &e) {
    std::cerr << e.what() << std::endl;
    if (e.code() != 400)
      throw std::runtime_error("test_way::033: Expected HTTP/400");
  }

}

void test_relation() {

  // Missing id
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972"><member type="node" ref="1" role="stop"/></relation></create></osmChange>)");
    throw std::runtime_error("test_relation::002: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::002: Expected HTTP/400");
  }

  // Missing ref
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1"><member type="node" role="stop"/></relation></create></osmChange>)");
    throw std::runtime_error("test_relation::002: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::002: Expected HTTP/400");
  }

  // Role is optional and may not trigger an error
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1"><member type="node" ref="-1"/></relation></create></osmChange>)");
  } catch (http::exception &e) {
    throw std::runtime_error("test_relation::003: Unexpected exception");
  }

  // Missing member type
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1"><member role="stop" ref="-1"/></relation></create></osmChange>)");
    throw std::runtime_error("test_relation::004: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::004: Expected HTTP/400");
  }

  // Invalid member type
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1"><member type="bla" role="stop" ref="-1"/></relation></create></osmChange>)");
    throw std::runtime_error("test_relation::005: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::005: Expected HTTP/400");
  }

  // Invalid ref
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1"><member type="node" ref="a" role="stop"/></relation></create></osmChange>)");
    throw std::runtime_error("test_relation::005: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::005: Expected HTTP/400");
  }

  // Invalid zero ref
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1"><member type="way" ref="0" role="stop"/></relation></create></osmChange>)");
    throw std::runtime_error("test_relation::006: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::006: Expected HTTP/400");
  }

  // ref too large
  try {
    process_testmsg(
        R"(<osmChange><create><relation changeset="972" id="-1">
           <member type="relation" ref="99999999999999999999999999999999" role="stop"/>
           </relation></create></osmChange>)");
    throw std::runtime_error("test_relation::007: Expected exception");
  } catch (http::exception &e) {
    if (e.code() != 400)
      throw std::runtime_error("test_relation::007: Expected HTTP/400");
  }


  // Member: role may have up to 255 unicode characters
  for (int i = 1; i <= 256; i++) {
    auto v = repeat("😎", i);

    try {
      process_testmsg(
          (boost::format(
               R"(<osmChange><create><relation changeset="858" id="-1">
                           <member type="node" role="%1%" ref="123"/>
                  </relation></create></osmChange>)") %
           v).str());
      if (i > 255)
        throw std::runtime_error("test_relation::010: Expected exception for "
                                 "string length > 255 unicode characters");
    } catch (http::exception &e) {
      if (e.code() != 400)
        throw std::runtime_error("test_relation::010: Expected HTTP/400");
      if (std::string(e.what()) !=
          "Relation Role has more than 255 unicode characters")
        throw std::runtime_error(
            "test_relation::010: Expected Relation Role has more than "
            "255 unicode characters");
      if (i <= 255)
        throw std::runtime_error("test_relation::010: Unexpected exception for "
                                 "string length <= 255 characters");
    }
  }
}

void test_large_message() {

  // Test XML chunking with a very large message
  std::stringstream s;

  s << "<osmChange>";

  for (int i = 1; i < 100000; i++) {

    switch (i % 3) {
    case 0:
      s << "<create>";
      s << R"(<node changeset="123" lat="1" lon="2" id="-)" << i
        << R"("><tag k="some key" v="some value"/></node>)";
      s << "</create>";
      break;
    case 1:
      s << "<modify>";
      s << R"(<node changeset="234" version="1" lat="1" lon="2" id=")" << i
        << R"("><tag k="some key" v="some value"/></node>)";
      s << "</modify>";
      break;
    case 2:
      s << "<delete>";
      s << R"(<node changeset="456" version="1" id=")" << i << R"("></node>)";
      s << "</delete>";
    }
  }

  s << "</osmChange>";

  // Valid message
  try {
    process_testmsg(s.str());
  } catch (http::exception &e) {
    throw std::runtime_error("test_large_message::001: Unexpected Exception");
  }
}

int main(int argc, char *argv[]) {
  try {
    test_osmchange_structure();
    test_node();
    test_way();
    test_relation();
    test_large_message();

  } catch (const std::exception &ex) {
    std::cerr << "EXCEPTION: " << ex.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN ERROR" << std::endl;
    return 1;
  }

  return 0;
}
