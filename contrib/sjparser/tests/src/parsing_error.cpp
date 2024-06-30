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

#include "sjparser/parsing_error.h"

#include <gtest/gtest.h>

using namespace SJParser;

TEST(ParsingError, SjparserError) {
  ParsingError error("Test error");

  ASSERT_EQ("Test error", error.sjparserError());
  ASSERT_EQ("", error.parserError());
  ASSERT_STREQ("Test error", error.what());
}

TEST(ParsingError, ParserError) {
  ParsingError error("", "Parser error");

  ASSERT_EQ("", error.sjparserError());
  ASSERT_EQ("Parser error", error.parserError());
  ASSERT_STREQ("Parser error", error.what());
}

TEST(ParsingError, BothErrors) {
  ParsingError error("Test error", "Parser error");

  ASSERT_EQ("Test error", error.sjparserError());
  ASSERT_EQ("Parser error", error.parserError());
  ASSERT_STREQ("Test error", error.what());
}
