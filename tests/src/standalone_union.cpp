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

TEST(StandaloneUnion, Empty) {
  std::string buf(R"({})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(StandaloneUnion, EmptyWithType) {
  std::string buf(R"({"type": 1})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Mandatory member #0 is not present", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                             {"type": 1}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, OptionalMember) {
  std::string buf(R"({"type": 1})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}},
                 Presence::Optional},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(0, parser.parser().currentMemberId());
  ASSERT_FALSE(parser.parser().parser<0>().isSet());
}

TEST(StandaloneUnion, OptionalMemberWithDefaultValue) {
  std::string buf(R"({"type": 1})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}},
                 Presence::Optional, std::tuple<bool>{false}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(0, parser.parser().currentMemberId());
  ASSERT_FALSE(parser.parser().parser<0>().isSet());
  ASSERT_EQ(std::tuple<bool>{false}, parser.parser().get<0>());
}

TEST(StandaloneUnion, Null) {
  std::string buf(R"(null)");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(StandaloneUnion, Reset) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}},
                                      Member{"integer", Value<int64_t>{}}}}},
          Member{2, Object{std::tuple{Member{"bool", Value<bool>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>());
  ASSERT_EQ(10, parser.parser().get<0>().get<1>());

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(StandaloneUnion, AllValuesMembers) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}},
                                      Member{"integer", Value<int64_t>{}}}}},
          Member{2,
                 Object{std::tuple{Member{"double", Value<double>{}},
                                   Member{"string", Value<std::string>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>());
  ASSERT_EQ(10, parser.parser().get<0>().get<1>());

  buf = R"({"type": 2, "double": 11.5, "string": "value"})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().parser<0>().isSet());
  ASSERT_TRUE(parser.parser().parser<1>().isSet());
  ASSERT_EQ(1, parser.parser().currentMemberId());

  ASSERT_EQ(11.5, parser.parser().get<1>().get<0>());
  ASSERT_EQ("value", parser.parser().get<1>().get<1>());
}

TEST(StandaloneUnion, StringType) {
  std::string buf(
      R"(
{
  "type": "1",
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<std::string>{}, "type",
      std::tuple{
          Member{"1", Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{"2", Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>());

  buf = R"(
{
  "type": "2",
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().parser<0>().isSet());
  ASSERT_TRUE(parser.parser().parser<1>().isSet());
  ASSERT_EQ(1, parser.parser().currentMemberId());

  ASSERT_EQ(100, parser.parser().get<1>().get<0>());
}

TEST(StandaloneUnion, IncorrectTypeType) {
  std::string buf(
      R"(
{
  "type": "1",
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Unexpected token string", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                         {   "type": "1",   "bool": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, IncorrectTypeValue) {
  std::string buf(
      R"(
{
  "type": 3,
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Unexpected member 3", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                           {   "type": 3,   "bool": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, IncorrectTypeMember) {
  std::string buf(
      R"(
{
  "error": 1,
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Unexpected member error", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                             {   "error": 1,   "bool": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, MembersWithCallbackError) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  auto boolCb = [&](decltype(parser)::ParserType::ParserType<0> &) {
    return false;
  };

  auto intCb = [&](decltype(parser)::ParserType::ParserType<1> &) {
    return false;
  };

  parser.parser().parser<0>().setFinishCallback(boolCb);
  parser.parser().parser<1>().setFinishCallback(intCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
             "type": 1,   "bool": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
           {   "type": 2,   "int": 100 }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, UnionWithCallback) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true
})");

  bool bool_value = false;
  int64_t int_value;

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  auto unionCb = [&](decltype(parser)::ParserType &parser) {
    if (parser.currentMemberId() == 0) {
      bool_value = parser.get<0>().get<0>();
    } else {
      int_value = parser.get<1>().get<0>();
    }
    return true;
  };

  parser.parser().setFinishCallback(unionCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>());
  ASSERT_EQ(true, bool_value);

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(100, parser.parser().get<1>().get<0>());
  ASSERT_EQ(100, int_value);
}

TEST(StandaloneUnion, UnionWithCallbackError) {
  std::string buf(R"(
{
  "type": 1,
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  auto unionCb = [&](decltype(parser)::ParserType &) { return false; };

  parser.parser().setFinishCallback(unionCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_TRUE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
             "type": 1,   "bool": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, UnionWithUnexpectedObject) {
  std::string buf(
      R"(
{
  "type": 1,
  "error": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Unexpected member error", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                {   "type": 1,   "error": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, RepeatingMembers) {
  try {
    Parser parser{Union{
        TypeHolder<int64_t>{}, "type",
        std::tuple{
            Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
            Member{1, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("Member 1 appears more, than once", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneUnion, StandaloneUnionWithParserReference) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true,
  "string": "value"
})");

  SAutoObject sautoobject{std::tuple{Member{"bool", Value<bool>{}},
                                     Member{"string", Value<std::string>{}}}};

  Parser parser{Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, sautoobject},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(parser.parser().get<0>()));
  ASSERT_EQ("value", std::get<1>(parser.parser().get<0>()));

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(100, parser.parser().get<1>().get<0>());

  ASSERT_EQ(&(parser.parser().parser<0>()), &sautoobject);
}

TEST(StandaloneUnion, MoveAssignment) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  auto union_parser_src = Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}},
                                      Member{"integer", Value<int64_t>{}}}}},
          Member{2,
                 Object{std::tuple{Member{"double", Value<double>{}},
                                   Member{"string", Value<std::string>{}}}}}}};
  auto union_parser = Union{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool_", Value<bool>{}},
                                      Member{"integer_", Value<int64_t>{}}}}},
          Member{2,
                 Object{std::tuple{Member{"double_", Value<double>{}},
                                   Member{"string_", Value<std::string>{}}}}}}};
  union_parser = std::move(union_parser_src);

  Parser parser{union_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>());
  ASSERT_EQ(10, parser.parser().get<0>().get<1>());
}
