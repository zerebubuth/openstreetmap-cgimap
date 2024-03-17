#include "parsers/parse_error.hpp"

namespace xmlpp {

parse_error::parse_error(const std::string& message)
: exception(message)
{
}

parse_error::~parse_error() noexcept
{}

void parse_error::raise() const
{
  throw *this;
}

exception* parse_error::clone() const
{
  return new parse_error(*this);
}

} //namespace xmlpp
