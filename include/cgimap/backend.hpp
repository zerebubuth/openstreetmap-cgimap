#ifndef BACKEND_HPP
#define BACKEND_HPP

#include <memory>
#include <string>
#include <ostream>
#include <boost/program_options.hpp>
#include "cgimap/data_update.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/oauth.hpp"

/* implement this interface to add a new backend which will be selectable
 * on the command line.
 */
struct backend {
  virtual ~backend();
  // the name of the backend, used as the option argument to select this in
  // --backend=
  virtual const std::string &name() const = 0;
  // the options that this backend can take. these are matched to command line
  // arguments and environment variables and passed into create() as a
  // variables_map.
  virtual const boost::program_options::options_description &
  options() const = 0;
  // create a data selection factory from the arguments passed to cgimap.
  virtual std::shared_ptr<data_selection::factory>
  create(const boost::program_options::variables_map &) = 0;
  // create a data update factory from the arguments passed to cgimap.
  virtual std::shared_ptr<data_update::factory>
  create_data_update(const boost::program_options::variables_map &) = 0;
  // create an oauth store based on arguments.
  virtual std::shared_ptr<oauth::store>
  create_oauth_store(const boost::program_options::variables_map &) = 0;
};

// figures out which backend should be selected and adds its options to the
// options description.
void setup_backend_options(int argc, char *argv[],
                           boost::program_options::options_description &);
// prints the options for all backends.
void output_backend_options(std::ostream &);
// singleton call to create a backend from a given set of options.
std::shared_ptr<data_selection::factory>
create_backend(const boost::program_options::variables_map &);

// singleton call to create a backend from a given set of options.
std::shared_ptr<data_update::factory>
create_update_backend(const boost::program_options::variables_map &);

// singleton call to create an OAuth store from options.
std::shared_ptr<oauth::store>
create_oauth_store(const boost::program_options::variables_map &);

// this function registers a backend for use when creating backends
// from user-provided options.
bool register_backend(std::shared_ptr<backend>);

#endif /* BACKEND_HPP */
