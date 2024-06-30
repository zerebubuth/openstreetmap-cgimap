/*******************************************************************************

Copyright (c) 2016-2017 Denis Tikhomirov <dvtikhomirov@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include <gtest/gtest.h>

#include <map>

#include "sjparser/sjparser.h"

using namespace SJParser;

TEST(Object, Empty) {
  std::string buf(R"({})");

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Object, Null) {
  std::string buf(R"(null)");

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Object, Reset) {
  std::string buf(R"({"bool": true, "string": "value"})");

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_EQ("value", parser.parser().get<1>());

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Object, UnexpectedMember) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected member error", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                {"error": true, "bool": true, "string"
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Object, IgnoredUnexpectedMember) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}},
                       ObjectOptions{Reaction::Ignore}}};

  parser.parse(buf);
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_EQ("value", parser.parser().get<1>());
}

TEST(Object, MembersWithCallbackError) {
  std::string buf(R"({"bool": true, "string": "value"})");

  auto boolCb = [&](const bool &) { return false; };

  auto stringCb = [&](const std::string &) { return true; };

  Parser parser{
      Object{std::tuple{Member{"bool", Value<bool>{boolCb}},
                        Member{"string", Value<std::string>{stringCb}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                           {"bool": true, "string": "value"}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Object, ObjectWithCallback) {
  std::string buf(R"({"bool": true, "string": "value"})");
  bool bool_value = false;
  std::string str_value;

  using ObjectParser = Object<Value<bool>, Value<std::string>>;

  auto objectCb = [&](ObjectParser &parser) {
    bool_value = parser.get<0>();
    str_value = parser.get<1>();
    return true;
  };

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}},
                       objectCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_EQ("value", parser.parser().get<1>());
  ASSERT_EQ(true, bool_value);
  ASSERT_EQ("value", str_value);
}

TEST(Object, ObjectWithOptionsAndCallback) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");
  bool bool_value = false;
  std::string str_value;

  using ObjectParser = Object<Value<bool>, Value<std::string>>;

  auto objectCb = [&](ObjectParser &parser) {
    bool_value = parser.get<0>();
    str_value = parser.get<1>();
    return true;
  };

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}},
                       ObjectOptions{Reaction::Ignore}, objectCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_EQ("value", parser.parser().get<1>());
  ASSERT_EQ(true, bool_value);
  ASSERT_EQ("value", str_value);
}

TEST(Object, ObjectWithCallbackError) {
  std::string buf(R"({"bool": true, "string": "value"})");

  using ObjectParser = Object<Value<bool>, Value<std::string>>;

  auto objectCb = [&](ObjectParser &) { return false; };

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}},
                       objectCb}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_TRUE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
          ool": true, "string": "value"}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Object, StdStringiMemberNames) {
  std::string buf(R"({"string": "value", "integer": 10})");

  std::string string_name = "string";
  std::string integer_name = "integer";

  Parser parser{Object{std::tuple{Member{string_name, Value<std::string>{}},
                                  Member{integer_name, Value<int64_t>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ("value", parser.parser().get<0>());
  ASSERT_EQ(10, parser.parser().get<1>());
}

TEST(Object, ObjectWithUnexpectedObject) {
  std::string buf(
      R"(
{
  "string": "value",
  "object": {
    "error": 1
  }
})");

  Parser parser{Object{std::tuple{
      Member{"string", Value<std::string>{}},
      Member{"object",
             Object{std::tuple{Member{"integer", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected member error", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
          ue",   "object": {     "error": 1   } }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Object, Move) {
  std::string buf(
      R"(
{
  "object": {
    "integer": 1,
    "string": "in_value"
  }
})");

  static bool copy_constructor_used = false;

  struct ObjectStruct {
    int64_t int_member;
    std::string str_member;

    ObjectStruct() { int_member = 0; }

    ObjectStruct(ObjectStruct &&other) {
      int_member = std::move(other.int_member);
      str_member = std::move(other.str_member);
    }

    // Needed for parser internals
    ObjectStruct &operator=(const ObjectStruct &other) {
      int_member = other.int_member;
      str_member = other.str_member;
      copy_constructor_used = true;
      return *this;
    }
  };

  Parser parser{Object{std::tuple{Member{
      "object",
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"integer", Value<int64_t>{}},
                               Member{"string", Value<std::string>{}}}}}}}};

  auto innerObjectCb = [&](decltype(parser)::ParserType::ParserType<0> &parser,
                           ObjectStruct &value) {
    value.int_member = parser.get<0>();
    value.str_member = parser.get<1>();
    return true;
  };

  parser.parser().parser<0>().setFinishCallback(innerObjectCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  copy_constructor_used = false;
  auto value = parser.parser().pop<0>();
  ASSERT_FALSE(parser.parser().parser<0>().isSet());

  ASSERT_FALSE(copy_constructor_used);
  ASSERT_EQ(1, value.int_member);
  ASSERT_EQ("in_value", value.str_member);

  buf = R"(
{
  "object": {
    "integer": 10,
    "string": "in_value2"
  }
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  copy_constructor_used = false;
  auto value2 = parser.parser().pop<0>();
  ASSERT_FALSE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(copy_constructor_used);

  ASSERT_EQ(10, value2.int_member);
  ASSERT_EQ("in_value2", value2.str_member);
}

TEST(Object, RepeatingMembers) {
  try {
    Parser parser{Object{std::tuple{Member{"member", Value<bool>{}},
                                    Member{"member", Value<std::string>{}}}}};

    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("Member member appears more than once", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Object, ObjectWithParserReference) {
  std::string buf(
      R"(
{
  "string": "value",
  "integer": 10,
  "array": [
    "elt1",
    "elt2",
    "elt3"
  ]
})");

  SArray sarray{Value<std::string>{}};

  Parser parser{Object{std::tuple{Member{"string", Value<std::string>{}},
                                  Member{"integer", Value<int64_t>{}},
                                  Member{"array", sarray}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ("value", parser.parser().get<0>());
  ASSERT_EQ(10, parser.parser().get<1>());
  ASSERT_EQ(3, parser.parser().get<2>().size());
  ASSERT_EQ("elt1", parser.parser().get<2>()[0]);
  ASSERT_EQ("elt2", parser.parser().get<2>()[1]);
  ASSERT_EQ("elt3", parser.parser().get<2>()[2]);

  ASSERT_EQ(&(parser.parser().parser<2>()), &sarray);
}

TEST(Object, MissingMember) {
  std::string buf(R"({"bool": true})");

  Parser parser{Object{std::tuple{Member{"bool", Value<bool>{}},
                                  Member{"string", Value<std::string>{}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Mandatory member string is not present", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                          {"bool": true}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Object, OptionalMember) {
  std::string buf(R"({"bool": true})");

  Parser parser{Object{
      std::tuple{Member{"bool", Value<bool>{}},
                 Member{"string", Value<std::string>{}, Presence::Optional}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
}

TEST(Object, OptionalMemberWithDefaultValue) {
  std::string buf(R"({"bool": true})");

  Parser parser{Object{std::tuple{
      Member{"bool", Value<bool>{}},
      Member{"string", Value<std::string>{}, Presence::Optional, "value"}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ("value", parser.parser().get<1>());

  std::string value = parser.parser().pop<1>();
  ASSERT_EQ("value", value);
}

TEST(Object, MoveAssignment) {
  std::string buf(R"({"bool": true, "string": "value"})");

  auto object_parser_src = Object{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}};
  auto object_parser = Object{std::tuple{
      Member{"bool_", Value<bool>{}}, Member{"string_", Value<std::string>{}}}};
  object_parser = std::move(object_parser_src);

  Parser parser{object_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_EQ("value", parser.parser().get<1>());
}

TEST(Object, StructuredBindings) {
  std::string buf(
      R"(
{
  "bool": true,
  "string": "value",
  "object": {
    "integer": 10
  }
})");

  Parser parser{Object{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}},
      Member{"object",
             Object{std::tuple{Member{"integer", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  auto &[bool_val, string_val, inner_parser] = parser.parser();

  ASSERT_EQ(true, bool_val);
  ASSERT_EQ("value", string_val);
  ASSERT_EQ(10, inner_parser.get<0>());
}
