#ifndef BACKEND_HPP
#define BACKEND_HPP

#include <string>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include "data_selection.hpp"

/*
 */
struct backend {
  virtual ~backend();
  virtual const std::string &name() const = 0;
  virtual const boost::program_options::options_description &options() const = 0;
  virtual boost::shared_ptr<data_selection::factory> create(const boost::program_options::variables_map &) = 0;
};

bool register_backend(boost::shared_ptr<backend>);
void setup_backend_options(boost::program_options::options_description &);
boost::shared_ptr<data_selection::factory> create_backend(const boost::program_options::variables_map &);

#endif /* BACKEND_HPP */
