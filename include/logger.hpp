#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>

class logger {
public:
  logger(void);
  ~logger(void);
  template <typename T> logger &operator<<(const T &t);
};

template<typename T> logger &
logger::operator<<(const T &t)
{
  std::cout << t;
  return *this;
}

#endif /* LOGGER_HPP */
