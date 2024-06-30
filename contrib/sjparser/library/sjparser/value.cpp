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

#include "value.h"

namespace SJParser {

template <typename ValueT>
Value<ValueT>::Value(Callback on_finish) : _on_finish{std::move(on_finish)} {
  // Formatting disabled because of a bug in clang-format
  // clang-format off
  if constexpr (std::is_same_v<ValueT, int64_t>
                || std::is_same_v<ValueT, bool>
                || std::is_same_v<ValueT, double>) {
    _value = 0;
  }
  // clang-format on
}

template <typename ValueT>
Value<ValueT>::Value(Value &&other) noexcept
    : TokenParser{std::move(other)},
      _value{std::move(other._value)},
      _on_finish{std::move(other._on_finish)} {}

template <typename ValueT>
Value<ValueT> &Value<ValueT>::operator=(Value &&other) noexcept {
  TokenParser::operator=(std::move(other));
  _value = std::move(other._value);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename ValueT>
void Value<ValueT>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename ValueT> void Value<ValueT>::on(TokenType<ValueT> value) {
  setNotEmpty();
  _value = value;
  endParsing();
}

template <typename ValueT>
void Value<ValueT>::on(TokenSecondaryType<ValueT> value) {
  if constexpr (!std::is_same_v<TokenSecondaryType<ValueT>, SJParser::DummyT>) {
    setNotEmpty();
    _value = value;
    endParsing();
  }
}

template <typename ValueT> void Value<ValueT>::finish() {
  if (_on_finish && !_on_finish(_value)) {
    throw std::runtime_error("Callback returned false");
  }
}

template <typename ValueT> const ValueT &Value<ValueT>::get() const {
  checkSet();
  return _value;
}

template <typename ValueT> ValueT &&Value<ValueT>::pop() {
  checkSet();
  unset();
  return std::move(_value);
}

template class Value<int64_t>;
template class Value<bool>;
template class Value<double>;
template class Value<std::string>;

}  // namespace SJParser
