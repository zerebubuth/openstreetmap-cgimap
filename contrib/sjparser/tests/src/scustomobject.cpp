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

#include "sjparser/sjparser.h"

using namespace SJParser;

TEST(SCustomObject, Empty) {
  std::string buf(R"({})");

  struct ObjectStruct {};

  auto objectCb = [&](auto &, ObjectStruct &) { return true; };

  Parser parser{SCustomObject{TypeHolder<ObjectStruct>{},
                              std::tuple{Member{"string", Value<std::string>{}},
                                         Member{"integer", Value<int64_t>{}}},
                              objectCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SCustomObject, Null) {
  std::string buf(R"(null)");

  struct ObjectStruct {};

  auto objectCb = [&](auto &, ObjectStruct &) { return true; };

  Parser parser{SCustomObject{TypeHolder<ObjectStruct>{},
                              std::tuple{Member{"string", Value<std::string>{}},
                                         Member{"integer", Value<int64_t>{}}},
                              objectCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SCustomObject, Reset) {
  std::string buf(R"({"bool": true, "string": "value"})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  auto objectCb = [&](auto &parser, ObjectStruct &value) {
    value.bool_value = parser.template get<0>();
    value.str_value = parser.template get<1>();
    return true;
  };

  Parser parser{
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool", Value<bool>{}},
                               Member{"string", Value<std::string>{}}},
                    objectCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get().bool_value);
  ASSERT_EQ("value", parser.parser().get().str_value);

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
}

TEST(SCustomObject, UnexpectedMember) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool", Value<bool>{}},
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

TEST(SCustomObject, IgnoredUnexpectedMember) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool", Value<bool>{}},
                               Member{"string", Value<std::string>{}}},
                    {Reaction::Ignore}}};

  auto objectCb = [&](decltype(parser)::ParserType &parser,
                      ObjectStruct &value) {
    value.bool_value = parser.get<0>();
    value.str_value = parser.get<1>();
    return true;
  };

  parser.parser().setFinishCallback(objectCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get().bool_value);
  ASSERT_EQ("value", parser.parser().get().str_value);
}

TEST(SCustomObject, MembersWithCallbackError) {
  std::string buf(R"({"bool": true, "string": "value"})");

  auto boolCb = [&](const bool &) { return false; };

  auto stringCb = [&](const std::string &) { return true; };

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{SCustomObject{
      TypeHolder<ObjectStruct>{},
      std::tuple{Member{"bool", Value<bool>{boolCb}},
                 Member{"string", Value<std::string>{stringCb}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
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

TEST(SCustomObject, SCustomObjectWithCallbackError) {
  std::string buf(R"({"bool": true, "string": "value"})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool", Value<bool>{}},
                               Member{"string", Value<std::string>{}}}}};

  auto objectCb = [&](decltype(parser)::ParserType &, ObjectStruct &) {
    return false;
  };

  parser.parser().setFinishCallback(objectCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
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

TEST(SCustomObject, PopValue) {
  std::string buf(R"({"string": "value", "integer": 10})");

  struct ObjectStruct {
    std::string str_value;
    int64_t int_value;
  };

  Parser parser{SCustomObject{TypeHolder<ObjectStruct>{},
                              std::tuple{Member{"string", Value<std::string>{}},
                                         Member{"integer", Value<int64_t>{}}}}};

  auto objectCb = [&](decltype(parser)::ParserType &parser,
                      ObjectStruct &value) {
    value.str_value = parser.get<0>();
    value.int_value = parser.get<1>();
    return true;
  };

  parser.parser().setFinishCallback(objectCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  auto value = parser.parser().pop();
  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_EQ("value", value.str_value);
  ASSERT_EQ(10, value.int_value);
}

TEST(SCustomObject, Move) {
  std::string buf(
      R"(
{
  "integer": 1,
  "string": "in_value"
})");

  struct ObjectStruct {
    int64_t int_member;
    std::string str_member;

    ObjectStruct() { int_member = 0; }

    ObjectStruct(ObjectStruct &&other) {
      int_member = std::move(other.int_member);
      str_member = std::move(other.str_member);
    }

    // Needed for parser internals
    ObjectStruct &operator=(ObjectStruct &&other) {
      int_member = std::move(other.int_member);
      str_member = std::move(other.str_member);
      return *this;
    }
  };

  Parser parser{
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"integer", Value<int64_t>{}},
                               Member{"string", Value<std::string>{}}}}};

  auto objectCb = [&](decltype(parser)::ParserType &parser,
                      ObjectStruct &value) {
    value.int_member = parser.get<0>();
    value.str_member = parser.get<1>();
    return true;
  };

  parser.parser().setFinishCallback(objectCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  auto value = parser.parser().pop();
  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_EQ(1, value.int_member);
  ASSERT_EQ("in_value", value.str_member);

  buf = R"(
{
  "integer": 10,
  "string": "in_value2"
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  auto value2 = parser.parser().pop();
  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_EQ(10, value2.int_member);
  ASSERT_EQ("in_value2", value2.str_member);
}

TEST(SCustomObject, SCustomObjectWithParserReference) {
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

  struct ObjectStruct {
    std::string str_value;
    int64_t int_value;
    std::vector<std::string> array_value;
  };

  SArray sarray{Value<std::string>{}};

  Parser parser{SCustomObject{TypeHolder<ObjectStruct>{},
                              std::tuple{Member{"string", Value<std::string>{}},
                                         Member{"integer", Value<int64_t>{}},
                                         Member{"array", sarray}}}};

  auto objectCb = [&](decltype(parser)::ParserType &parser,
                      ObjectStruct &value) {
    value.str_value = parser.get<0>();
    value.int_value = parser.get<1>();
    value.array_value = parser.get<2>();
    return true;
  };

  parser.parser().setFinishCallback(objectCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ("value", parser.parser().get().str_value);
  ASSERT_EQ(10, parser.parser().get().int_value);
  ASSERT_EQ(3, parser.parser().get().array_value.size());
  ASSERT_EQ("elt1", parser.parser().get().array_value[0]);
  ASSERT_EQ("elt2", parser.parser().get().array_value[1]);
  ASSERT_EQ("elt3", parser.parser().get().array_value[2]);

  ASSERT_EQ(&(parser.parser().parser<2>()), &sarray);
}

TEST(SCustomObject, MissingMember) {
  std::string buf(R"({"bool": true})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool", Value<bool>{}},
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

TEST(SCustomObject, OptionalMember) {
  std::string buf(R"({"bool": true})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{SCustomObject{
      TypeHolder<ObjectStruct>{},
      std::tuple{Member{"bool", Value<bool>{}},
                 Member{"string", Value<std::string>{}, Presence::Optional}}}};

  parser.parse(buf);
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
}

TEST(SCustomObject, OptionalMemberWithDefaultValue) {
  std::string buf(R"({"bool": true})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  Parser parser{SCustomObject{TypeHolder<ObjectStruct>{},
                              std::tuple{Member{"bool", Value<bool>{}},
                                         Member{"string", Value<std::string>{},
                                                Presence::Optional, "value"}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ("value", parser.parser().get<1>());
}

TEST(SCustomObject, MoveAssignmenteset) {
  std::string buf(R"({"bool": true, "string": "value"})");

  struct ObjectStruct {
    bool bool_value;
    std::string str_value;
  };

  auto objectCb = [&](auto &parser, ObjectStruct &value) {
    value.bool_value = parser.template get<0>();
    value.str_value = parser.template get<1>();
    return true;
  };

  auto scustom_object_parser_src =
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool", Value<bool>{}},
                               Member{"string", Value<std::string>{}}},
                    objectCb};
  auto scustom_object_parser =
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"bool_", Value<bool>{}},
                               Member{"string_", Value<std::string>{}}}};
  scustom_object_parser = std::move(scustom_object_parser_src);

  Parser parser{scustom_object_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get().bool_value);
  ASSERT_EQ("value", parser.parser().get().str_value);
}
