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

TEST(SMap, Empty) {
  std::string buf(R"({})");

  Parser parser{SMap{Value<bool>{}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(0, parser.parser().get().size());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SMap, EmptyWithCallbacks) {
  std::string buf(R"({})");
  bool key_callback_called = false;
  bool finish_callback_called = false;

  Parser parser{SMap{Value<bool>{}}};

  auto elementCb = [&](const std::string &,
                       decltype(parser)::ParserType::ParserType &) {
    key_callback_called = true;
    return true;
  };

  auto finishCb = [&](decltype(parser)::ParserType &) {
    finish_callback_called = true;
    return true;
  };

  parser.parser().setElementCallback(elementCb);
  parser.parser().setFinishCallback(finishCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(0, parser.parser().get().size());
  ASSERT_TRUE(parser.parser().isSet());

  ASSERT_FALSE(key_callback_called);
  ASSERT_TRUE(finish_callback_called);
}

TEST(SMap, Null) {
  std::string buf(R"(null)");

  Parser parser{SMap{Value<bool>{}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SMap, Reset) {
  std::string buf(R"({"1": true})");

  bool value = false;

  Parser parser{SMap{Value<bool>{}}};

  auto elementCb = [&](const std::string &,
                       decltype(parser)::ParserType::ParserType &parser) {
    value = parser.get();
    return true;
  };

  parser.parser().setElementCallback(elementCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(value);

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());

  ASSERT_EQ(1, parser.parser().get().size());
  ASSERT_EQ(true, parser.parser().get().at("1"));

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(SMap, SeveralKeys) {
  std::string buf(R"({"1": 10, "2": 15})");

  Parser parser{SMap{Value<int64_t>{}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, parser.parser().get().size());
  ASSERT_EQ(10, parser.parser().get().at("1"));
  ASSERT_EQ(15, parser.parser().get().at("2"));
}

TEST(SMap, KeyCallbackError) {
  std::string buf(R"({"1": 10})");

  auto elementCb = [&](const std::string &, Value<int64_t> &) { return false; };

  Parser parser{SMap{Value<int64_t>{}, elementCb}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Element callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                {"1": 10}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(SMap, FinishCallbackError) {
  std::string buf(R"({"1": 10})");

  Parser parser{SMap{Value<int64_t>{}}};

  auto finishCb = [&](decltype(parser)::ParserType &) { return false; };

  parser.parser().setFinishCallback(finishCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_TRUE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                               {"1": 10}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(SMap, SMapOfSMaps) {
  std::string buf(
      R"({
  "1": {
    "1": [10, 20],
    "2": [30, 40]
  },
  "2": {
    "1": [11, 21],
    "2": [31, 41]
  }
})");

  Parser parser{SMap{SMap{SArray{Value<int64_t>{}}}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, parser.parser().get().size());

  ASSERT_EQ(2, parser.parser().get().at("1").size());
  ASSERT_EQ(10, parser.parser().get().at("1").at("1")[0]);
  ASSERT_EQ(20, parser.parser().get().at("1").at("1")[1]);
  ASSERT_EQ(30, parser.parser().get().at("1").at("2")[0]);
  ASSERT_EQ(40, parser.parser().get().at("1").at("2")[1]);

  ASSERT_EQ(2, parser.parser().get().at("2").size());
  ASSERT_EQ(11, parser.parser().get().at("2").at("1")[0]);
  ASSERT_EQ(21, parser.parser().get().at("2").at("1")[1]);
  ASSERT_EQ(31, parser.parser().get().at("2").at("2")[0]);
  ASSERT_EQ(41, parser.parser().get().at("2").at("2")[1]);
}

TEST(SMap, SMapWithParserReference) {
  std::string buf(R"({
  "1": [10, 20],
  "2": [30, 40]
})");

  SArray sarray{Value<int64_t>{}};

  Parser parser{SMap{sarray}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, parser.parser().get().size());
  ASSERT_EQ(10, parser.parser().get().at("1")[0]);
  ASSERT_EQ(20, parser.parser().get().at("1")[1]);
  ASSERT_EQ(30, parser.parser().get().at("2")[0]);
  ASSERT_EQ(40, parser.parser().get().at("2")[1]);

  ASSERT_EQ(&(parser.parser().parser()), &sarray);
}

// Just check if the constructor compiles
TEST(SMap, SMapWithMapReference) {
  SMap array{Value<int64_t>{}};

  Parser parser{SMap{array}};

  ASSERT_EQ(&(parser.parser().parser()), &array);
}

TEST(SMap, MoveAssignment) {
  std::string buf(R"({"1": 10, "2": 15})");

  auto smap_parser_src = SMap{Value<int64_t>{}};
  auto smap_parser = SMap{Value<int64_t>{}};
  smap_parser = std::move(smap_parser_src);

  Parser parser{smap_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, parser.parser().get().size());
  ASSERT_EQ(10, parser.parser().get().at("1"));
  ASSERT_EQ(15, parser.parser().get().at("2"));
}
