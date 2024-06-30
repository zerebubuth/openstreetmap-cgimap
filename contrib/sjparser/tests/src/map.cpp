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

#include <map>

#include "sjparser/sjparser.h"

using namespace SJParser;

TEST(Map, Empty) {
  std::string buf(R"({})");

  Parser parser{Map{Value<bool>{}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Map, EmptyWithCallbacks) {
  std::string buf(R"({})");
  bool key_callback_called = false;
  bool finish_callback_called = false;

  Parser parser{Map{Value<bool>{}}};

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

  ASSERT_TRUE(parser.parser().isSet());

  ASSERT_FALSE(key_callback_called);
  ASSERT_TRUE(finish_callback_called);
}

TEST(Map, Null) {
  std::string buf(R"(null)");

  Parser parser{Map{Value<bool>{}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Map, Reset) {
  std::string buf(R"({"1": true})");

  bool value = false;

  Parser parser{Map{Value<bool>{}}};

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

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Map, SeveralKeys) {
  std::string buf(R"({"1": 10, "2": 15})");

  std::map<std::string, int64_t> values;

  Parser parser{Map{Value<int64_t>{}}};

  auto elementCb = [&](const std::string &key,
                       decltype(parser)::ParserType::ParserType &parser) {
    values[key] = parser.get();
    return true;
  };

  parser.parser().setElementCallback(elementCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(10, values["1"]);
  ASSERT_EQ(15, values["2"]);
}

TEST(Map, InternalCallbackError) {
  std::string buf(R"({"1": 10})");

  auto internalCb = [&](const int64_t &) { return false; };

  Parser parser{Map{Value<int64_t>{internalCb}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

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

TEST(Map, KeyCallbackError) {
  std::string buf(R"({"1": 10})");

  auto elementCb = [&](const std::string &, Value<int64_t> &) { return false; };

  Parser parser{Map{Value<int64_t>{}, elementCb}};

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

TEST(Map, FinishCallbackError) {
  std::string buf(R"({"1": 10})");

  Parser parser{Map{Value<int64_t>{}}};

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

TEST(Map, MapOfMaps) {
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

  std::map<std::string, std::map<std::string, std::vector<int64_t>>> values;
  std::map<std::string, std::vector<int64_t>> tmp_values;

  Parser parser{Map{Map{SArray{Value<int64_t>{}}}}};

  auto innerElementCb =
      [&](const std::string &key,
          decltype(parser)::ParserType::ParserType::ParserType &parser) {
        tmp_values[key] = parser.pop();
        return true;
      };

  auto elementCb = [&](const std::string &key,
                       decltype(parser)::ParserType::ParserType &) {
    values[key] = tmp_values;
    tmp_values.clear();
    return true;
  };

  parser.parser().parser().setElementCallback(innerElementCb);

  parser.parser().setElementCallback(elementCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values["1"].size());
  ASSERT_EQ(10, values["1"]["1"][0]);
  ASSERT_EQ(20, values["1"]["1"][1]);
  ASSERT_EQ(30, values["1"]["2"][0]);
  ASSERT_EQ(40, values["1"]["2"][1]);

  ASSERT_EQ(2, values["2"].size());
  ASSERT_EQ(11, values["2"]["1"][0]);
  ASSERT_EQ(21, values["2"]["1"][1]);
  ASSERT_EQ(31, values["2"]["2"][0]);
  ASSERT_EQ(41, values["2"]["2"][1]);
}

TEST(Map, MapWithParserReference) {
  std::string buf(R"({
  "1": [10, 20],
  "2": [30, 40]
})");

  std::map<std::string, std::vector<int64_t>> values;

  SArray sarray{Value<int64_t>{}};

  Parser parser{Map{sarray}};

  auto elementCb = [&](const std::string &key,
                       decltype(parser)::ParserType::ParserType &parser) {
    values[key] = parser.pop();
    return true;
  };

  parser.parser().setElementCallback(elementCb);

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(10, values["1"][0]);
  ASSERT_EQ(20, values["1"][1]);
  ASSERT_EQ(30, values["2"][0]);
  ASSERT_EQ(40, values["2"][1]);

  ASSERT_EQ(&(parser.parser().parser()), &sarray);
}

// Just check if the constructor compiles
TEST(Map, MapWithMapReference) {
  Map array{Value<int64_t>{}};

  Parser parser{Map{array}};

  ASSERT_EQ(&(parser.parser().parser()), &array);
}

TEST(Map, MoveAssignment) {
  std::string buf(R"({"1": 10, "2": 15})");
  std::map<std::string, int64_t> values;

  auto elementCb = [&](const std::string &key, Value<int64_t> &parser) {
    values[key] = parser.get();
    return true;
  };
  auto map_parser_src = Map{Value<int64_t>{}, elementCb};
  auto map_parser = Map{Value<int64_t>{}};
  map_parser = std::move(map_parser_src);

  Parser parser{map_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(10, values["1"]);
  ASSERT_EQ(15, values["2"]);
}
