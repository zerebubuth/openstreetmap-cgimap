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

#include <functional>

#include "internals/token_parser.h"

namespace SJParser {

/** @brief Plain value parser.
 *
 * @tparam ValueT JSON value type, can be std::string, int64_t, bool or double
 */

template <typename ValueT> class Value : public TokenParser {
 public:
  /** Underlying type, that can be obtained from this parser with #get or #pop.
   */
  using ValueType = ValueT;

  /** Finish callback type. */
  using Callback = std::function<bool(const ValueType &)>;

  /** @brief Constructor.
   *
   * @param [in] on_finish (optional) Callback, that will be called after a
   * value is parsed.
   * The callback will be called with a const reference to a parsed value as an
   * argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  explicit Value(Callback on_finish = nullptr);

  /** Move constructor. */
  Value(Value &&other) noexcept;

  /** Move assignment operator */
  Value<ValueT> &operator=(Value &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~Value() override = default;
  Value(const Value &) = delete;
  Value &operator=(const Value &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after a
   * value is parsed.
   * The callback will be called with a const reference to a parsed value as an
   *
   * If the callback returns false, parsing will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

  /** @brief Parsed value getter.
   *
   * @return Const reference to a parsed value.
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  [[nodiscard]] const ValueType &get() const;

  /** @brief Get the parsed value and unset the parser.
   *
   * Moves the parsed value out of the parser.
   *
   * @return Rvalue reference to the parsed value.
   *
   * @throw std::runtime_error thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  ValueType &&pop();

 private:
  void on(TokenType<ValueType> value) override;
  void on(TokenSecondaryType<ValueType> value) override;
  void finish() override;

  ValueType _value;
  Callback _on_finish;
};

extern template class Value<int64_t>;
extern template class Value<bool>;
extern template class Value<double>;
extern template class Value<std::string>;

}  // namespace SJParser
