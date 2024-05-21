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

TEST(StandaloneSUnion, Empty) {
  std::string buf(R"({})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(StandaloneSUnion, EmptyWithType) {
  std::string buf(R"({"type": 1})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Can not set value: Mandatory member #0 is not present",
              e.sjparserError());

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

TEST(StandaloneSUnion, OptionalMember) {
  std::string buf(R"({"type": 1})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}},
                 Presence::Optional},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ(
        "Can not set value: Optional member #0 does not have a default value",
        e.sjparserError());

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

TEST(StandaloneSUnion, OptionalMemberWithDefaultValue) {
  std::string buf(R"({"type": 1})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}},
                 Presence::Optional, std::tuple<bool>{false}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  auto variant = parser.parser().get();

  ASSERT_EQ(0, variant.index());

  ASSERT_EQ(false, std::get<0>(std::get<0>(variant)));
}

TEST(StandaloneSUnion, Null) {
  std::string buf(R"(null)");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(StandaloneSUnion, Reset) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1,
                 SAutoObject{std::tuple{Member{"bool", Value<bool>{}},
                                        Member{"integer", Value<int64_t>{}}}}},
          Member{2, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  auto variant = parser.parser().get();

  ASSERT_EQ(0, variant.index());

  auto object = std::get<0>(variant);

  ASSERT_EQ(true, std::get<0>(object));
  ASSERT_EQ(10, std::get<1>(object));

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(StandaloneSUnion, AllValuesMembers) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{Member{1, SAutoObject{std::tuple{
                               Member{"bool", Value<bool>{}},
                               Member{"integer", Value<int64_t>{}}}}},
                 Member{2, SAutoObject{std::tuple{
                               Member{"double", Value<double>{}},
                               Member{"string", Value<std::string>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  {
    auto variant = parser.parser().get();

    ASSERT_EQ(0, variant.index());

    auto object = std::get<0>(variant);

    ASSERT_EQ(true, std::get<0>(object));
    ASSERT_EQ(10, std::get<1>(object));
  }

  buf = R"({"type": 2, "double": 11.5, "string": "value"})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  {
    auto variant = parser.parser().get();

    ASSERT_EQ(1, variant.index());

    auto object = std::get<1>(variant);

    ASSERT_EQ(11.5, std::get<0>(object));
    ASSERT_EQ("value", std::get<1>(object));
  }
}

TEST(StandaloneSUnion, StringType) {
  std::string buf(
      R"(
{
  "type": "1",
  "bool": true
})");

  Parser parser{SUnion{
      TypeHolder<std::string>{}, "type",
      std::tuple{
          Member{"1", SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{"2",
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  {
    auto variant = parser.parser().get();

    ASSERT_EQ(0, variant.index());

    ASSERT_EQ(true, std::get<0>(std::get<0>(variant)));
  }

  buf = R"(
{
  "type": "2",
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  {
    auto variant = parser.parser().get();

    ASSERT_EQ(1, variant.index());

    ASSERT_EQ(100, std::get<0>(std::get<1>(variant)));
  }
}

TEST(StandaloneSUnion, IncorrectTypeType) {
  std::string buf(
      R"(
{
  "type": "1",
  "bool": true
})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

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

TEST(StandaloneSUnion, IncorrectTypeValue) {
  std::string buf(
      R"(
{
  "type": 3,
  "bool": true
})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

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

TEST(StandaloneSUnion, IncorrectTypeMember) {
  std::string buf(
      R"(
{
  "error": 1,
  "bool": true
})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

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

TEST(StandaloneSUnion, MembersWithCallbackError) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true
})");

  auto boolCb = [&](const std::tuple<bool> &) { return false; };

  auto intCb = [&](const std::tuple<int64_t> &) { return false; };

  Parser parser{
      SUnion{TypeHolder<int64_t>{}, "type",
             std::tuple{Member{1, SAutoObject{std::tuple{
                                      Member{"bool", Value<bool>{boolCb}}}}},
                        Member{2, SAutoObject{std::tuple{Member{
                                      "int", Value<int64_t>{intCb}}}}}}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
           {   "type": 1,   "bool": true }
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

TEST(StandaloneSUnion, SUnionWithCallback) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true
})");

  bool bool_value = false;
  int64_t int_value;

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  auto unionCb =
      [&](const std::variant<std::tuple<bool>, std::tuple<int64_t>> &value) {
        if (value.index() == 0) {
          bool_value = std::get<0>(std::get<0>(value));
        } else {
          int_value = std::get<0>(std::get<1>(value));
        }
        return true;
      };

  parser.parser().setFinishCallback(unionCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(std::get<0>(parser.parser().get())));
  ASSERT_EQ(true, bool_value);

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(100, std::get<0>(std::get<1>(parser.parser().get())));
  ASSERT_EQ(100, int_value);
}

TEST(StandaloneSUnion, SUnionWithCallbackError) {
  std::string buf(R"(
{
  "type": 1,
  "bool": true
})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

  auto unionCb =
      [&](const std::variant<std::tuple<bool>, std::tuple<int64_t>> &) {
        return false;
      };

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

TEST(StandaloneSUnion, SUnionWithUnexpectedObject) {
  std::string buf(
      R"(
{
  "type": 1,
  "error": true
})");

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SAutoObject{std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

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

template <typename Member1, typename Member2> struct MoveStruct {
  Member1 member1;
  Member2 member2;
  static bool copy_used;

  MoveStruct() {
    if constexpr (std::is_integral_v<
                      Member1> || std::is_floating_point_v<Member1>) {
      member1 = 0;
    }
    if constexpr (std::is_integral_v<
                      Member2> || std::is_floating_point_v<Member2>) {
      member2 = 0;
    }
  }

  MoveStruct(const MoveStruct<Member1, Member2> &other) {
    member1 = std::move(other.member1);
    member2 = std::move(other.member2);
    copy_used = true;
  }

  MoveStruct &operator=(const MoveStruct<Member1, Member2> &other) {
    member1 = std::move(other.member1);
    member2 = std::move(other.member2);
    copy_used = true;
    return *this;
  }

  MoveStruct(MoveStruct<Member1, Member2> &&other) {
    member1 = std::move(other.member1);
    member2 = std::move(other.member2);
  }
};

template <typename Member1, typename Member2>
bool MoveStruct<Member1, Member2>::copy_used = false;

TEST(StandaloneSUnion, Move) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  using ObjectStruct1 = MoveStruct<bool, int64_t>;
  using ObjectStruct2 = MoveStruct<double, std::string>;

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SCustomObject{TypeHolder<ObjectStruct1>{},
                                  std::tuple{
                                      Member{"bool", Value<bool>{}},
                                      Member{"integer", Value<int64_t>{}}}}},
          Member{2, SCustomObject{
                        TypeHolder<ObjectStruct2>{},
                        std::tuple{Member{"double", Value<double>{}},
                                   Member{"string", Value<std::string>{}}}}}}}};

  auto objectCb1 = [&](decltype(parser)::ParserType::ParserType<0> &parser,
                       auto &value) {
    value.member1 = parser.get<0>();
    value.member2 = parser.get<1>();
    return true;
  };

  auto objectCb2 = [&](decltype(parser)::ParserType::ParserType<1> &parser,
                       auto &value) {
    value.member1 = parser.get<0>();
    value.member2 = parser.get<1>();
    return true;
  };

  parser.parser().parser<0>().setFinishCallback(objectCb1);
  parser.parser().parser<1>().setFinishCallback(objectCb2);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ObjectStruct1::copy_used = false;

  {
    auto variant = parser.parser().pop();
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_FALSE((ObjectStruct1::copy_used));

    ASSERT_EQ(0, variant.index());

    auto &object = std::get<0>(variant);

    ASSERT_EQ(true, object.member1);
    ASSERT_EQ(10, object.member2);
  }

  buf = R"({"type": 2, "double": 11.5, "string": "value"})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ObjectStruct2::copy_used = false;

  {
    auto variant = parser.parser().pop();
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_FALSE((ObjectStruct2::copy_used));

    ASSERT_EQ(1, variant.index());

    auto &object = std::get<1>(variant);

    ASSERT_EQ(11.5, object.member1);
    ASSERT_EQ("value", object.member2);
  }
}

TEST(StandaloneSUnion, UnknownExceptionInValueSetter) {
  std::string buf(R"({"type": 1, "bool": true})");

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

  Parser parser{SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{
          Member{1, SCustomObject{TypeHolder<ObjectStruct>{},
                                  std::tuple{Member{"bool", Value<bool>{}}}}},
          Member{2,
                 SAutoObject{std::tuple{Member{"int", Value<int64_t>{}}}}}}}};

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
               {"type": 1, "bool": true}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(StandaloneSUnion, StandaloneSUnionWithParserReference) {
  std::string buf(
      R"(
{
  "type": 1,
  "bool": true,
  "string": "value"
})");

  SAutoObject sautoobject{std::tuple{Member{"bool", Value<bool>{}},
                                     Member{"string", Value<std::string>{}}}};

  Parser parser{SUnion{TypeHolder<int64_t>{}, "type",
                       std::tuple{Member{1, sautoobject},
                                  Member{2, SAutoObject{std::tuple{Member{
                                                "int", Value<int64_t>{}}}}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(true, std::get<0>(std::get<0>(parser.parser().get())));
  ASSERT_EQ("value", std::get<1>(std::get<0>(parser.parser().get())));

  buf = R"(
{
  "type": 2,
  "int": 100
})";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(100, std::get<0>(std::get<1>(parser.parser().get())));

  ASSERT_EQ(&(parser.parser().parser<0>()), &sautoobject);
}

TEST(StandaloneSUnion, MoveAssignment) {
  std::string buf(R"({"type": 1, "bool": true, "integer": 10})");

  auto sunion_parser_src = SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{Member{1, SAutoObject{std::tuple{
                               Member{"bool", Value<bool>{}},
                               Member{"integer", Value<int64_t>{}}}}},
                 Member{2, SAutoObject{std::tuple{
                               Member{"double", Value<double>{}},
                               Member{"string", Value<std::string>{}}}}}}};
  auto sunion_parser = SUnion{
      TypeHolder<int64_t>{}, "type",
      std::tuple{Member{1, SAutoObject{std::tuple{
                               Member{"bool_", Value<bool>{}},
                               Member{"integer_", Value<int64_t>{}}}}},
                 Member{2, SAutoObject{std::tuple{
                               Member{"double_", Value<double>{}},
                               Member{"string_", Value<std::string>{}}}}}}};
  sunion_parser = std::move(sunion_parser_src);

  Parser parser{sunion_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  auto variant = parser.parser().get();

  ASSERT_EQ(0, variant.index());

  auto object = std::get<0>(variant);

  ASSERT_EQ(true, std::get<0>(object));
  ASSERT_EQ(10, std::get<1>(object));
}
