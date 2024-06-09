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

TEST(Array, Empty) {
  std::string buf(R"([])");
  std::vector<bool> values;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(0, values.size());

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Array, Null) {
  std::string buf(R"(null)");
  std::vector<bool> values;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(0, values.size());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Array, Reset) {
  std::string buf(R"([true])");
  std::vector<bool> values;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(1, values.size());
  ASSERT_EQ(true, values[0]);

  ASSERT_TRUE(parser.parser().isSet());
  ASSERT_FALSE(parser.parser().isEmpty());

  buf = R"(null)";

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_FALSE(parser.parser().isSet());
  ASSERT_TRUE(parser.parser().isEmpty());
}

TEST(Array, ArrayOfBooleans) {
  std::string buf(R"([true, false])");
  std::vector<bool> values;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(true, values[0]);
  ASSERT_EQ(false, values[1]);

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, ArrayOfIntegers) {
  std::string buf(R"([10, 11])");
  std::vector<int64_t> values;

  auto elementCb = [&](const int64_t &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<int64_t>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(10, values[0]);
  ASSERT_EQ(11, values[1]);

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, ArrayOfDoubles) {
  std::string buf(R"([10.5, 11.2])");
  std::vector<double> values;

  auto elementCb = [&](const double &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<double>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(10.5, values[0]);
  ASSERT_EQ(11.2, values[1]);

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, ArrayOfStrings) {
  std::string buf(R"(["value1", "value2"])");
  std::vector<std::string> values;

  auto elementCb = [&](const std::string &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<std::string>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ("value1", values[0]);
  ASSERT_EQ("value2", values[1]);

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, ArrayWithNull) {
  std::string buf(R"([null])");
  std::vector<bool> values;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(0, values.size());

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, ArrayWithNullAndValues) {
  std::string buf(R"([null, true, null, false])");
  std::vector<bool> values;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(true, values[0]);
  ASSERT_EQ(false, values[1]);

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, UnexpectedBoolean) {
  std::string buf(R"(true)");

  auto elementCb = [&](const bool &) { return true; };

  Parser parser{Array{Value<bool>{elementCb}}};

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

TEST(Array, UnexpectedInteger) {
  std::string buf(R"(10)");

  auto elementCb = [&](const int64_t &) { return true; };

  Parser parser{Array{Value<int64_t>{elementCb}}};

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

TEST(Array, UnexpectedDouble) {
  std::string buf(R"(10.5)");

  auto elementCb = [&](const double &) { return true; };

  Parser parser{Array{Value<double>{elementCb}}};

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

TEST(Array, UnexpectedString) {
  std::string buf(R"("value")");

  auto elementCb = [&](const std::string &) { return true; };

  Parser parser{Array{Value<std::string>{elementCb}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token string", e.sjparserError());

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

TEST(Array, ArrayOfObjects) {
  std::string buf(
      R"([{"key": "value", "key2": 10}, {"key": "value2", "key2": 20}])");

  struct ObjectStruct {
    std::string member1;
    int64_t member2;
  };

  std::vector<ObjectStruct> values;

  using ObjectParser = Object<Value<std::string>, Value<int64_t>>;

  auto objectCb = [&](ObjectParser &parser) {
    values.push_back({parser.pop<0>(), parser.pop<1>()});
    return true;
  };

  Parser parser{Array{Object{std::tuple{Member{"key", Value<std::string>{}},
                                        Member{"key2", Value<int64_t>{}}},
                             objectCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ("value", values[0].member1);
  ASSERT_EQ(10, values[0].member2);
  ASSERT_EQ("value2", values[1].member1);
  ASSERT_EQ(20, values[1].member2);
}

TEST(Array, UnexpectedMapStart) {
  std::string buf(R"({})");

  Parser parser{Array{Value<bool>{}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token map start", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                       {}
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Array, ArrayWithUnexpectedType) {
  std::string buf(R"([true])");

  auto elementCb = [&](const std::string &) { return true; };

  Parser parser{Array{Value<std::string>{elementCb}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());
    ASSERT_EQ("Unexpected token boolean", e.sjparserError());

    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                   [true]
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Array, ArrayWithElementCallbackError) {
  std::string buf(R"([true, false])");

  auto elementCb = [&](const bool &) { return false; };

  Parser parser{Array{Value<bool>{elementCb}}};

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_FALSE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                                   [true, false]
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Array, ArrayWithCallback) {
  std::string buf(R"([true, false])");
  std::vector<bool> values;
  bool callback_called = false;

  auto elementCb = [&](const bool &value) {
    values.push_back(value);
    return true;
  };

  auto arrayCb = [&](Array<Value<bool>> &) {
    callback_called = true;
    return true;
  };

  Parser parser{Array{Value<bool>{elementCb}, arrayCb}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(true, values[0]);
  ASSERT_EQ(false, values[1]);

  ASSERT_EQ(true, callback_called);

  ASSERT_TRUE(parser.parser().isSet());
}

TEST(Array, ArrayWithCallbackError) {
  std::string buf(R"([true, false])");

  auto elementCb = [&](const bool &) { return true; };

  Parser parser{Array{Value<bool>{elementCb}}};

  auto arrayCb = [&](decltype(parser)::ParserType &) { return false; };

  parser.parser().setFinishCallback(arrayCb);

  try {
    parser.parse(buf);
    FAIL() << "No exception thrown";
  } catch (ParsingError &e) {
    ASSERT_TRUE(parser.parser().isSet());

    ASSERT_EQ("Callback returned false", e.sjparserError());
    ASSERT_EQ(
        R"(parse error: client cancelled parse via callback return value
                           [true, false]
                     (right here) ------^
)",
        e.parserError());
  } catch (...) {
    FAIL() << "Invalid exception thrown";
  }
}

TEST(Array, ArrayOfArrays) {
  std::string buf(R"([[true, true], [false, false]])");
  std::vector<std::vector<bool>> values;
  std::vector<bool> tmp_values;

  auto elementCb = [&](const bool &value) {
    tmp_values.push_back(value);
    return true;
  };

  auto innerArrayCb = [&](Array<Value<bool>> &) {
    values.push_back(tmp_values);
    tmp_values.clear();
    return true;
  };

  Parser parser{Array{Array{Value<bool>{elementCb}, innerArrayCb}}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(2, values[0].size());
  ASSERT_EQ(true, values[0][0]);
  ASSERT_EQ(true, values[0][1]);

  ASSERT_EQ(2, values[1].size());
  ASSERT_EQ(false, values[1][0]);
  ASSERT_EQ(false, values[1][1]);
}

TEST(Array, ArrayWithParserReference) {
  std::string buf(R"([[13, 15, 16]])");

  SArray sarray{Value<int64_t>{}};

  Parser parser{Array{sarray}};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(3, sarray.get().size());
  ASSERT_EQ(13, sarray.get()[0]);
  ASSERT_EQ(15, sarray.get()[1]);
  ASSERT_EQ(16, sarray.get()[2]);

  ASSERT_EQ(&(parser.parser().parser()), &sarray);
}

// Just check if the constructor compiles
TEST(Array, ArrayWithArrayReference) {
  Array array{Value<int64_t>{}};

  Parser parser{Array{array}};

  ASSERT_EQ(&(parser.parser().parser()), &array);
}

TEST(Array, MoveAssignment) {
  std::string buf(R"([10, 11])");
  std::vector<int64_t> values;

  auto elementCb = [&](const int64_t &value) {
    values.push_back(value);
    return true;
  };
  auto array_parser_src = Array{Value<int64_t>{elementCb}};
  auto array_parser = Array{Value<int64_t>{}};
  array_parser = std::move(array_parser_src);

  Parser parser{array_parser};

  ASSERT_NO_THROW(parser.parse(buf));
  ASSERT_NO_THROW(parser.finish());

  ASSERT_EQ(2, values.size());
  ASSERT_EQ(10, values[0]);
  ASSERT_EQ(11, values[1]);

  ASSERT_TRUE(parser.parser().isSet());
}
