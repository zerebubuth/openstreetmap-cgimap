#ifndef INFIX_OSTREAM_ITERATOR
#define INFIX_OSTREAM_ITERATOR

#include <ios>
#include <ostream>
#include <iterator>

/**
 * output iterator which infixes its delimiter string between the output items.
 */
template <typename _Tp, typename _CharT = char,
          typename _Traits = std::char_traits<_CharT> >
class infix_ostream_iterator
    : public std::iterator<std::output_iterator_tag, void, void, void, void> {
public:
  typedef _CharT char_type;
  typedef _Traits traits_type;
  typedef std::basic_ostream<_CharT, _Traits> ostream_type;

private:
  ostream_type *_M_stream;
  const _CharT *_M_string;
  bool _M_first;

public:
  infix_ostream_iterator(ostream_type &s)
      : _M_stream(&s), _M_string(0), _M_first(true) {}
  infix_ostream_iterator(ostream_type &s, const _CharT *c)
      : _M_stream(&s), _M_string(c), _M_first(true) {}
  infix_ostream_iterator(const infix_ostream_iterator &obj)
      : _M_stream(obj._M_stream),
        _M_string(obj._M_string),
        _M_first(obj._M_first) {}

  infix_ostream_iterator &operator=(const _Tp &value) {
    __glibcxx_requires_cond(
        _M_stream != 0,
        _M_message(__gnu_debug::__msg_output_ostream)._M_iterator(*this));
    if ((_M_string != 0) && (_M_first == false)) {
      *_M_stream << _M_string;
    } else {
      _M_first = false;
    }
    *_M_stream << value;
    return *this;
  }

  infix_ostream_iterator &operator*() { return *this; }
  infix_ostream_iterator &operator++() { return *this; }
  infix_ostream_iterator &operator++(int) { return *this; }
};

#endif /* INFIX_OSTREAM_ITERATOR */
