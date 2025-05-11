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

TEST(OptionalValue, Boolean) {
  std::string buf(R"(true)");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, Integer) {
  std::string buf(R"(10)");

  Parser parser{OptionalValue<int64_t>{}};

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

TEST(OptionalValue, Double) {
  std::string buf(R"(1.3)");

  Parser parser{OptionalValue<double>{}};

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

TEST(OptionalValue, String) {
  std::string buf(R"("value")");

  Parser parser{OptionalValue<std::string>{}};

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

TEST(OptionalValue, Null) {
  std::string buf(R"(null)");

  Parser parser{OptionalValue<bool>{}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(OptionalValue, Reset) {
  std::string buf(R"(true)");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, UnexpectedBoolean) {
  std::string buf(R"(true)");

  Parser parser{OptionalValue<std::string>{}};

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

TEST(OptionalValue, UnexpectedString) {
  std::string buf(R"("error")");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, UnexpectedInteger) {
  std::string buf(R"(10)");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, UnexpectedDouble) {
  std::string buf(R"(10.5)");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, UnexpectedMapStart) {
  std::string buf(R"({)");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, UnexpectedMapKey) {
  Parser parser{OptionalValue<bool>{}, TypeHolder<TestParser>{}};

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

TEST(OptionalValue, UnexpectedMapEnd) {
  Parser parser{OptionalValue<bool>{}, TypeHolder<TestParser>{}};

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

TEST(OptionalValue, UnexpectedArrayStart) {
  std::string buf(R"([)");

  Parser parser{OptionalValue<bool>{}};

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

TEST(OptionalValue, UnexpectedArrayEnd) {
  Parser parser{OptionalValue<bool>{}, TypeHolder<TestParser>{}};

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

TEST(OptionalValue, UnsetValue) {
  Parser parser{OptionalValue<bool>{}};

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_EQ(parser.parser().get(), std::optional<bool>{});
}

TEST(OptionalValue, ValueWithCallback) {
  std::string buf(R"("value")");
  std::string value;

  auto elementCb = [&](const std::optional<std::string> &str) {
    value = str.value();
    return true;
  };

  Parser parser{OptionalValue<std::string>{elementCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_EQ("value", parser.parser().get());
  ASSERT_EQ("value", value);
}

TEST(OptionalValue, ValueWithCallbackError) {
  std::string buf(R"("value")");

  auto elementCb = [&](const std::optional<std::string> &) { return false; };

  Parser parser{OptionalValue<std::string>{}};

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

TEST(OptionalValue, MoveAssignment) {
  std::string buf(R"(10)");

  auto value_parser_src = OptionalValue<int64_t>{};
  auto value_parser = OptionalValue<int64_t>{};
  value_parser = std::move(value_parser_src);

  Parser parser{value_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());
  ASSERT_EQ(10, parser.parser().get());
}
