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

TEST(SAutoObject, Empty) {
  std::string buf(R"({})");

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SAutoObject, Null) {
  std::string buf(R"(null)");

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SAutoObject, Reset) {
  std::string buf(R"({"bool": true, "string": "value"})");

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(parser.parser().get()));
  ASSERT_EQ("value", std::get<1>(parser.parser().get()));

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
}

TEST(SAutoObject, UnexpectedMember) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}}};

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

TEST(SAutoObject, IgnoredUnexpectedMember) {
  std::string buf(R"({"error": true, "bool": true, "string": "value"})");

  Parser parser{SAutoObject{std::tuple{Member{"bool", Value<bool>{}},
                                       Member{"string", Value<std::string>{}}},
                            {Reaction::Ignore}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(parser.parser().get()));
  ASSERT_EQ("value", std::get<1>(parser.parser().get()));
}

TEST(SAutoObject, MembersWithCallbackError) {
  std::string buf(R"({"bool": true, "string": "value"})");

  auto boolCb = [&](const bool &) { return false; };

  auto stringCb = [&](const std::string &) { return true; };

  Parser parser{
      SAutoObject{std::tuple{Member{"bool", Value<bool>{boolCb}},
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

TEST(SAutoObject, SAutoObjectWithCallback) {
  std::string buf(R"({"bool": true, "string": "value"})");

  using ValueType = std::tuple<bool, std::string>;
  ValueType value;

  auto objectCb = [&](const ValueType &_value) {
    value = _value;
    return true;
  };

  Parser parser{SAutoObject{std::tuple{Member{"bool", Value<bool>{}},
                                       Member{"string", Value<std::string>{}}},
                            objectCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(value));
  ASSERT_EQ("value", std::get<1>(value));
}

TEST(SAutoObject, SAutoObjectWithCallbackError) {
  std::string buf(R"({"bool": true, "string": "value"})");

  auto objectCb = [&](const std::tuple<bool, std::string> &) { return false; };

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}}};

  parser.parser().setFinishCallback(objectCb);

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

TEST(SAutoObject, Move) {
  std::string buf(
      R"(
{
  "object": {
    "integer": 1,
    "string": "in_value"
  }
})");
  static bool copy_used = false;

  struct ObjectStruct {
    int64_t int_member;
    std::string str_member;

    ObjectStruct() { int_member = 0; }

    ObjectStruct(const ObjectStruct &other) {
      int_member = other.int_member;
      str_member = other.str_member;
      copy_used = true;
    }

    ObjectStruct &operator=(const ObjectStruct &other) {
      int_member = other.int_member;
      str_member = other.str_member;
      copy_used = true;
      return *this;
    }

    ObjectStruct(ObjectStruct &&other) {
      int_member = std::move(other.int_member);
      str_member = std::move(other.str_member);
    }
  };

  Parser parser{SAutoObject{std::tuple{Member{
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

  copy_used = false;

  auto value = parser.parser().pop();
  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_FALSE(copy_used);
  ASSERT_EQ(1, std::get<0>(value).int_member);
  ASSERT_EQ("in_value", std::get<0>(value).str_member);

  buf = R"(
{
  "object": {
    "integer": 10,
    "string": "in_value2"
  }
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  copy_used = false;

  auto value2 = parser.parser().pop();
  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_FALSE(copy_used);
  ASSERT_EQ(10, std::get<0>(value2).int_member);
  ASSERT_EQ("in_value2", std::get<0>(value2).str_member);
}

TEST(SAutoObject, UnknownExceptionInValueSetter) {
  std::string buf(
      R"(
{
  "object": {
    "integer": 1,
    "string": "in_value"
  }
})");

  struct ObjectStruct {
    bool throw_on_assign = false;

    ObjectStruct() {}

    ObjectStruct(const ObjectStruct &) {}

    ObjectStruct &operator=(const ObjectStruct &other) {
      throw_on_assign = other.throw_on_assign;
      if (throw_on_assign) {
        throw 10;
      }
      return *this;
    }
  };

  Parser parser{SAutoObject{std::tuple{Member{
      "object",
      SCustomObject{TypeHolder<ObjectStruct>{},
                    std::tuple{Member{"integer", Value<int64_t>{}},
                               Member{"string", Value<std::string>{}}}}}}}};

  auto innerObjectCb = [&](decltype(parser)::ParserType::ParserType<0> &,
                           ObjectStruct &object) {
    object.throw_on_assign = true;
    return true;
  };

  parser.parser().parser<0>().setFinishCallback(innerObjectCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Can not set value: unknown exception", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
              "string": "in_value"   } }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(SAutoObject, SAutoObjectWithParserReference) {
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

  Parser parser{SAutoObject{std::tuple{Member{"string", Value<std::string>{}},
                                       Member{"integer", Value<int64_t>{}},
                                       Member{"array", sarray}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ("value", std::get<0>(parser.parser().get()));
  ASSERT_EQ(10, std::get<1>(parser.parser().get()));

  auto array = std::get<2>(parser.parser().get());
  ASSERT_EQ(3, array.size());
  ASSERT_EQ("elt1", array[0]);
  ASSERT_EQ("elt2", array[1]);
  ASSERT_EQ("elt3", array[2]);

  ASSERT_EQ(&(parser.parser().parser<2>()), &sarray);
}

TEST(SAutoObject, MissingMember) {
  std::string buf(R"({"bool": true})");

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Can not set value: Mandatory member string is not present",
              e.sjparserError());

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

TEST(SAutoObject, OptionalMember) {
  std::string buf(R"({"bool": true})");

  Parser parser{SAutoObject{
      std::tuple{Member{"bool", Value<bool>{}},
                 Member{"string", Value<std::string>{}, Presence::Optional}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ(
        "Can not set value: Optional member string does not have a default "
        "value",
        e.sjparserError());

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

TEST(SAutoObject, OptionalMemberWithDefaultValue) {
  std::string buf(R"({"bool": true})");

  Parser parser{SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}},
      Member{"string", Value<std::string>{}, Presence::Optional, "value"}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(parser.parser().get()));
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ("value", std::get<1>(parser.parser().get()));
}

TEST(SAutoObject, MoveAssignment) {
  std::string buf(R"({"bool": true, "string": "value"})");

  auto sauto_object_parser_src = SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}};
  auto sauto_object_parser = SAutoObject{std::tuple{
      Member{"bool", Value<bool>{}}, Member{"string", Value<std::string>{}}}};
  sauto_object_parser = std::move(sauto_object_parser_src);

  Parser parser{sauto_object_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(parser.parser().get()));
  ASSERT_EQ("value", std::get<1>(parser.parser().get()));
}
