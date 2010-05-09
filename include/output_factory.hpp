#ifndef OUTPUT_FACTORY_HPP
#define OUTPUT_FACTORY_HPP

#include "output_formatter.hpp"
#include "output_buffer.hpp"
#include "cache.hpp"

#include <boost/shared_ptr.hpp>

/**
 * factory for creating writers and formatters for output formats.
 */
struct output_factory {
  virtual ~output_factory();
  virtual writer &create_writer(boost::shared_ptr<output_buffer> &out, bool indent = true) = 0;
  virtual output_formatter &create_formatter(cache<long int, changeset> &changeset_cache) = 0;
};

#endif /* OUTPUT_FACTORY_HPP */
