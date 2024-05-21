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
#include "test_parser.h"

using namespace SJParser;

TEST(EmbeddedUnion, Empty) {
  std::string buf(R"({"type": 1})");

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().parser<0>().isSet());
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

TEST(EmbeddedUnion, OptionalMember) {
  std::string buf(R"({"type": 1})");

  Parser parser{Object{std::tuple{Member{
      "type",
      Union{TypeHolder<int64_t>{},
            std::tuple{
                Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}},
                       Presence::Optional},
                Member{2, Object{std::tuple{
                              Member{"int", Value<int64_t>{}}}}}}}}}}};
  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().parser<0>().isEmpty());
  ASSERT_EQ(0, parser.parser().parser<0>().currentMemberId());
  ASSERT_FALSE(parser.parser().parser<0>().parser<0>().isSet());
}

TEST(EmbeddedUnion, OptionalMemberWithDefaultValue) {
  std::string buf(R"({"type": 1})");

  Parser parser{Object{std::tuple{Member{
      "type",
      Union{
          TypeHolder<int64_t>{},
          std::tuple{
              Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}},
                     Presence::Optional, std::tuple<bool>{false}},
              Member{2,
                     Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().parser<0>().isEmpty());
  ASSERT_EQ(0, parser.parser().parser<0>().currentMemberId());
  ASSERT_FALSE(parser.parser().parser<0>().parser<0>().isSet());
  ASSERT_EQ(std::tuple<bool>{false}, parser.parser().parser<0>().get<0>());
}

TEST(EmbeddedUnion, Reset) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  Parser parser{Object{std::tuple{Member{
      "type",
      Union{TypeHolder<int64_t>{},
            std::tuple{Member{1, Object{std::tuple{
                                     Member{"bool", Value<bool>{}},
                                     Member{"integer", Value<int64_t>{}}}}},
                       Member{2, Object{std::tuple{
                                     Member{"bool", Value<bool>{}}}}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().get<0>().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().parser<0>().isEmpty());
  ASSERT_FALSE(parser.parser().get<0>().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().get<0>().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>().get<0>());
  ASSERT_EQ(10, parser.parser().get<0>().get<0>().get<1>());

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().parser<0>().isSet());
  ASSERT_TRUE(parser.parser().parser<0>().isEmpty());
}

TEST(EmbeddedUnion, AllValuesMembers) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  Parser parser{Object{std::tuple{Member{
      "type",
      Union{TypeHolder<int64_t>{},
            std::tuple{
                Member{1, Object{std::tuple{
                              Member{"bool", Value<bool>{}},
                              Member{"integer", Value<int64_t>{}}}}},
                Member{2, Object{std::tuple{
                              Member{"double", Value<double>{}},
                              Member{"string", Value<std::string>{}}}}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().get<0>().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().get<0>().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().get<0>().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>().get<0>());
  ASSERT_EQ(10, parser.parser().get<0>().get<0>().get<1>());

  buf = R"({"type": 2, "double": 11.5, "string": "value"})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().get<0>().parser<0>().isSet());
  ASSERT_TRUE(parser.parser().get<0>().parser<1>().isSet());
  ASSERT_EQ(1, parser.parser().get<0>().currentMemberId());

  ASSERT_EQ(11.5, parser.parser().get<0>().get<1>().get<0>());
  ASSERT_EQ("value", parser.parser().get<0>().get<1>().get<1>());
}

TEST(EmbeddedUnion, StringType) {
  std::string buf(
      R"(
{
  "type": "1",
  "bool": true
})");

  Parser parser{Object{std::tuple{Member{
      "type",
      Union{TypeHolder<std::string>{},
            std::tuple{
                Member{"1", Object{std::tuple{Member{"bool", Value<bool>{}}}}},
                Member{"2", Object{std::tuple{
                                Member{"int", Value<int64_t>{}}}}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().get<0>().parser<0>().isSet());
  ASSERT_FALSE(parser.parser().get<0>().parser<1>().isSet());
  ASSERT_EQ(0, parser.parser().get<0>().currentMemberId());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>().get<0>());

  buf = R"(
{
  "type": "2",
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().get<0>().parser<0>().isSet());
  ASSERT_TRUE(parser.parser().get<0>().parser<1>().isSet());
  ASSERT_EQ(1, parser.parser().get<0>().currentMemberId());

  ASSERT_EQ(100, parser.parser().get<0>().get<1>().get<0>());
}

TEST(EmbeddedUnion, IncorrectTypeType) {
  std::string buf(
      R"(
{
  "type": "1",
  "bool": true
})");

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().parser<0>().isSet());

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

TEST(EmbeddedUnion, IncorrectTypeValue) {
  std::string buf(
      R"(
{
  "type": 3,
  "bool": true
})");

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().parser<0>().isSet());

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

TEST(EmbeddedUnion, MembersWithCallbackError) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true
})");

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  auto boolCb =
      [&](decltype(parser)::ParserType::ParserType<0>::ParserType<0> &) {
        return false;
      };

  auto intCb =
      [&](decltype(parser)::ParserType::ParserType<0>::ParserType<1> &) {
        return false;
      };

  parser.parser().parser<0>().parser<0>().setFinishCallback(boolCb);
  parser.parser().parser<0>().parser<1>().setFinishCallback(intCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().parser<0>().isSet());

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
    ASSERT_FALSE(parser.parser().parser<0>().isSet());

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

TEST(EmbeddedUnion, UnionWithCallback) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true
})");

  bool bool_value = false;
  int64_t int_value;

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  auto unionCb = [&](decltype(parser)::ParserType::ParserType<0> &parser) {
    if (parser.currentMemberId() == 0) {
      bool_value = parser.get<0>().get<0>();
    } else {
      int_value = parser.get<1>().get<0>();
    }
    return true;
  };

  parser.parser().parser<0>().setFinishCallback(unionCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, parser.parser().get<0>().get<0>().get<0>());
  ASSERT_EQ(true, bool_value);

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(100, parser.parser().get<0>().get<1>().get<0>());
  ASSERT_EQ(100, int_value);
}

TEST(EmbeddedUnion, UnionWithCallbackError) {
  std::string buf(R"(
{
  "type": 1,
  "bool": true
})");

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  auto unionCb = [&](decltype(parser)::ParserType::ParserType<0> &) {
    return false;
  };

  parser.parser().parser<0>().setFinishCallback(unionCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_TRUE(parser.parser().parser<0>().isSet());

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

TEST(EmbeddedUnion, UnionWithUnexpectedObject) {
  std::string buf(
      R"(
{
  "type": 1,
  "error": true
})");

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, Object{std::tuple{
                                             Member{"bool", Value<bool>{}}}}},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().parser<0>().isSet());

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

TEST(EmbeddedUnion, UnionWithUnexpectedMapStart) {
  std::string buf(R"(
{
  "type": 1,
  "bool": true
})");

  Parser parser{Union{
      TypeHolder<int64_t>{},
      std::tuple{
          Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Union with an empty type member can't parse this",
              e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                       {   "type": 1,   "bool": true }
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(EmbeddedUnion, UnionWithUnexpectedMapKey) {
  Parser parser{
      Union{
          TypeHolder<int64_t>{},
          std::tuple{
              Member{1, Object{std::tuple{Member{"bool", Value<bool>{}}}}},
              Member{2, Object{std::tuple{Member{"int", Value<int64_t>{}}}}}}},
      TypeHolder<TestParser>{}};

  auto test = [](TestParser *parser) {
    parser->dispatcher->on(MapKeyT{"test"});
  };

  try {
    parser.run(test);
    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_STREQ("Union with an empty type member can't parse this", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(EmbeddedUnion, EmbeddedUnionWithParserReference) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true,
  "string": "value"
})");

  SAutoObject sautoobject{std::tuple{Member{"bool", Value<bool>{}},
                                     Member{"string", Value<std::string>{}}}};

  Parser parser{Object{std::tuple{Member{
      "type", Union{TypeHolder<int64_t>{},
                    std::tuple{Member{1, sautoobject},
                               Member{2, Object{std::tuple{Member{
                                             "int", Value<int64_t>{}}}}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(parser.parser().get<0>().get<0>()));
  ASSERT_EQ("value", std::get<1>(parser.parser().get<0>().get<0>()));

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(100, parser.parser().get<0>().get<1>().get<0>());

  ASSERT_EQ(&(parser.parser().parser<0>().parser<0>()), &sautoobject);
}
