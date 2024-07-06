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

TEST(Member, MandatoryWithRvalueParser) {
  Member member{"test", Value<bool>{}};

  ASSERT_EQ(member.name, "test");
  ASSERT_FALSE(member.optional);
  ASSERT_FALSE(std::is_reference_v<decltype(member.parser)>);
}

TEST(Member, MandatoryWithLvalueParser) {
  Value<bool> parser;
  Member member{"test", parser};

  ASSERT_EQ(member.name, "test");
  ASSERT_FALSE(member.optional);
  ASSERT_TRUE(std::is_lvalue_reference_v<decltype(member.parser)>);
}

TEST(Member, MandatoryWithConstCharName) {
  Member member{"test", Value<bool>{}};

  ASSERT_TRUE((std::is_same_v<decltype(member.name), std::string>));
}

TEST(Member, MandatoryWithBoolName) {
  Member member{true, Value<bool>{}};

  ASSERT_TRUE((std::is_same_v<decltype(member.name), bool>));
}

TEST(Member, OptionalWithRvalueParser) {
  Member member{"test", Value<bool>{}, Presence::Optional};

  ASSERT_EQ(member.name, "test");
  ASSERT_TRUE(member.optional);
  ASSERT_FALSE(member.default_value.value);
  ASSERT_FALSE(std::is_reference_v<decltype(member.parser)>);
}

TEST(Member, OptionalWithLvalueParser) {
  Value<bool> parser;
  Member member{"test", parser, Presence::Optional};

  ASSERT_EQ(member.name, "test");
  ASSERT_TRUE(member.optional);
  ASSERT_FALSE(member.default_value.value);
  ASSERT_TRUE(std::is_lvalue_reference_v<decltype(member.parser)>);
}

TEST(Member, DefaultWithRvalueParser) {
  Member member{"test", Value<int64_t>{}, Presence::Optional, 10};

  ASSERT_EQ(member.name, "test");
  ASSERT_TRUE(member.optional);
  ASSERT_TRUE(member.default_value.value);
  ASSERT_EQ(*member.default_value.value, 10);
  ASSERT_FALSE(std::is_reference_v<decltype(member.parser)>);
}

TEST(Member, DefaultWithLvalueParser) {
  Value<int64_t> parser;
  Member member{"test", parser, Presence::Optional, 10};

  ASSERT_EQ(member.name, "test");
  ASSERT_TRUE(member.optional);
  ASSERT_TRUE(member.default_value.value);
  ASSERT_EQ(*member.default_value.value, 10);
  ASSERT_TRUE(std::is_lvalue_reference_v<decltype(member.parser)>);
}

TEST(Member, MoveAssignment) {
  Member member_src{"test", Value<int64_t>{}, Presence::Optional, 10};
  Member member{"test", Value<int64_t>{}};
  member = std::move(member_src);

  ASSERT_EQ(member.name, "test");
  ASSERT_TRUE(member.optional);
  ASSERT_TRUE(member.default_value.value);
  ASSERT_EQ(*member.default_value.value, 10);
  ASSERT_FALSE(std::is_reference_v<decltype(member.parser)>);
}

TEST(Member, StructuredBindings) {
  Member member{"test", Value<int64_t>{}, Presence::Optional, 10};

  auto& [name, parser, optional, default_value] = member;

  ASSERT_EQ(name, "test");
  ASSERT_TRUE(optional);
  ASSERT_TRUE(default_value.value);
  ASSERT_EQ(*default_value.value, 10);
}
