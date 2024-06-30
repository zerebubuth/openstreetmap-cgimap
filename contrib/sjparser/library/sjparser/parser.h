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

#pragma once

#include "type_holder.h"
#include "yajl_parser.h"

namespace SJParser {

/** @brief Main parser.
 *
 * @tparam ParserT Root parser (Value, Object, SAutoObject, SCustomObject
 * Union, SUnion, Array, SArray)
 * @anchor Parser_T
 *
 * @tparam ImplHolder Underlying parser implementation, the default one is
 * YajlParser, a YAJL parser adapter. This class inherits from the
 * implementation, so please refer to it's documentation for API details.
 */

template <typename ParserT, typename ImplHolder = TypeHolder<YajlParser>>
class Parser : public ImplHolder::type {
 public:
  /** Root parser type. */
  using ParserType = std::decay_t<ParserT>;

  /** @brief Constructor.
   *
   * @param [in] parser Root parser, can be an lvalue reference or an rvalue.
   *
   * @param [in] implementation Implementation class, please use a TypeHolder
   * wrapper for it.
   */
  explicit Parser(ParserT &&parser,
                  ImplHolder implementation = TypeHolder<YajlParser>{});

  /** @brief Root parser getter.
   *
   * @return A reference to the root parser (@ref Parser_T "ParserT").
   */
  [[nodiscard]] ParserType &parser();

 private:
  ParserT _parser;
};

template <typename ParserT> Parser(ParserT &&) -> Parser<ParserT>;

/****************************** Implementations *******************************/

template <typename ParserT, typename ImplHolder>
Parser<ParserT, ImplHolder>::Parser(ParserT &&parser,
                                    ImplHolder /*implementation*/)
    : _parser{std::forward<ParserT>(parser)} {
  static_assert(std::is_base_of_v<TokenParser, ParserType>,
                "Invalid parser used in Parser");
  this->setTokenParser(&_parser);
}

template <typename ParserT, typename ImplHolder>
typename Parser<ParserT, ImplHolder>::ParserType &
Parser<ParserT, ImplHolder>::parser() {
  return _parser;
}

}  // namespace SJParser
