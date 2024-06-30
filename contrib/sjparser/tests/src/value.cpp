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

#include <tuple>

#include "sjparser/sjparser.h"
#include "test_parser.h"

using namespace SJParser;

TEST(Value, Boolean) {
  std::string buf(R"(true)");

  Parser parser{Value<bool>{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(true, parser.parser().get());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_EQ(true, parser.parser().pop());
  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Value, Integer) {
  std::string buf(R"(10)");

  Parser parser{Value<int64_t>{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(10, parser.parser().get());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_EQ(10, parser.parser().pop());
  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Value, Double) {
  std::string buf(R"(1.3)");

  Parser parser{Value<double>{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(1.3, parser.parser().get());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_EQ(1.3, parser.parser().pop());
  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Value, String) {
  std::string buf(R"("value")");

  Parser parser{Value<std::string>{}};

  ASSERT_FALSE(parser.parser().isSet());

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ("value", parser.parser().get());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_EQ("value", parser.parser().pop());
  ASSERT_FALSE(parser.parser().isSet());
}

TEST(Value, Null) {
  std::string buf(R"(null)");

  Parser parser{Value<bool>{}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Value, Reset) {
  std::string buf(R"(true)");

  Parser parser{Value<bool>{}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(true, parser.parser().get());

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Value, UnexpectedBoolean) {
  std::string buf(R"(true)");

  Parser parser{Value<std::string>{}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token boolean", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                    true
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, UnexpectedString) {
  std::string buf(R"("error")");

  Parser parser{Value<bool>{}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token string", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                 "error"
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, UnexpectedInteger) {
  std::string buf(R"(10)");

  Parser parser{Value<bool>{}};

  ASSERT_NO_THROW(parser.parse(buf));
  try {
    parser.finish();
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token integer", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                        10
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, UnexpectedDouble) {
  std::string buf(R"(10.5)");

  Parser parser{Value<bool>{}};

  ASSERT_NO_THROW(parser.parse(buf));
  try {
    parser.finish();
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token double", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                        10.5
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, UnexpectedMapStart) {
  std::string buf(R"({)");

  Parser parser{Value<bool>{}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token map start", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                       {
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, UnexpectedMapKey) {
  Parser parser{Value<bool>{}, TypeHolder<TestParser>{}};

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

TEST(Value, UnexpectedMapEnd) {
  Parser parser{Value<bool>{}, TypeHolder<TestParser>{}};

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

TEST(Value, UnexpectedArrayStart) {
  std::string buf(R"([)");

  Parser parser{Value<bool>{}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token array start", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                       [
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, UnexpectedArrayEnd) {
  Parser parser{Value<bool>{}, TypeHolder<TestParser>{}};

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

TEST(Value, UnsetValue) {
  Parser parser{Value<bool>{}};

  ASSERT_FALSE(parser.parser().isSet());
  try {
    std::ignore = parser.parser().get();
    FAIL() << "No exception thrown";
  } catch (std::runtime_error &e) {
    ASSERT_STREQ("Can't get value, parser is unset", e.what());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, ValueWithCallback) {
  std::string buf(R"("value")");
  std::string value;

  auto elementCb = [&](const std::string &str) {
    value = str;
    return true;
  };

  Parser parser{Value<std::string>{elementCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_EQ("value", parser.parser().get());
  ASSERT_EQ("value", value);
}

TEST(Value, ValueWithCallbackError) {
  std::string buf(R"("value")");

  auto elementCb = [&](const std::string &) { return false; };

  Parser parser{Value<std::string>{}};

  parser.parser().setFinishCallback(elementCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                 "value"
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Value, MoveAssignment) {
  std::string buf(R"(10)");

  auto value_parser_src = Value<int64_t>{};
  auto value_parser = Value<int64_t>{};
  value_parser = std::move(value_parser_src);

  Parser parser{value_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(10, parser.parser().get());
}
