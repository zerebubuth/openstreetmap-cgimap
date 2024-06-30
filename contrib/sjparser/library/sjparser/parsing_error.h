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

#include <exception>
#include <string>

namespace SJParser {

/** @brief Parsing error exception.
 */
class ParsingError : public std::exception {
 public:
  /** @brief ParsingError constructor.
   *
   * @param [in] sjparser_error Sets sjparser error.
   *
   * @param [in] parser_error Sets underlying parser error.
   */
  explicit ParsingError(std::string sjparser_error,
                        std::string parser_error = "");

  /** @brief Error getter.
   *
   * @return SJParser or underlying parser error.
   */
  [[nodiscard]] const char *what() const noexcept override;

  /** @brief SJParser error getter.
   *
   * @return SJParser error.
   */
  [[nodiscard]] const std::string &sjparserError() const noexcept;

  /** @brief Underlying parser error getter.
   *
   * @return Underlying parser error.
   */
  [[nodiscard]] const std::string &parserError() const noexcept;

 private:
  std::string _sjparser_error;
  std::string _parser_error;
};
}  // namespace SJParser
