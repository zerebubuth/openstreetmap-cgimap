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

TEST(Ignore, Boolean) {
  std::string buf(R"(true)");

  Parser parser{Ignore{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Ignore, Integer) {
  std::string buf(R"(10)");

  Parser parser{Ignore{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Ignore, Double) {
  std::string buf(R"(1.3)");

  Parser parser{Ignore{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Ignore, String) {
  std::string buf(R"("value")");

  Parser parser{Ignore{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Ignore, Null) {
  std::string buf(R"(null)");

  Parser parser{Ignore{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Ignore, Reset) {
  std::string buf(R"(true)");

  Parser parser{Ignore{}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Ignore, Object) {
  std::string buf(R"({"bool": true, "string": "value"})");

  Parser parser{Ignore{}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Ignore, Array) {
  std::string buf(R"(["value1", "value2"])");

  Parser parser{Ignore{}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Ignore, UnexpectedMapKey) {
  Parser parser{Ignore{}, TypeHolder<TestParser>{}};

  auto test = [](TestParser *parser) {
    parser->dispatcher->on(MapKeyT{"test"});
  };

  try {
    parser.run(test);
    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("Unexpected token map key", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Ignore, UnexpectedMapEnd) {
  Parser parser{Ignore{}, TypeHolder<TestParser>{}};

  auto test = [](TestParser *parser) { parser->dispatcher->on(MapEndT()); };

  try {
    parser.run(test);
    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("Unexpected token map end", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Ignore, UnexpectedArrayEnd) {
  Parser parser{Ignore{}, TypeHolder<TestParser>{}};

  auto test = [](TestParser *parser) { parser->dispatcher->on(ArrayEndT()); };

  try {
    parser.run(test);
    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("Unexpected token array end", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}
