#include "backend.hpp"
#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

namespace po = boost::program_options;
using boost::shared_ptr;

namespace {
struct registry {
  bool add(shared_ptr<backend> ptr);
  void setup_options(po::options_description &desc);
  shared_ptr<data_selection::factory> create(const po::variables_map &options);

private:
  typedef std::map<std::string, shared_ptr<backend> > backend_map_t;
  backend_map_t backends;
  shared_ptr<backend> default_backend;
};

bool registry::add(shared_ptr<backend> ptr) {
  backends[ptr->name()] = ptr;
  return true;
}

void registry::setup_options(po::options_description &desc) {
  std::ostringstream all_backends;
  std::string description = (boost::format("backend to use, available options are %1%") % all_backends).str();
  desc.add_options()
    ("backend", po::value<std::string>()->default_value(default_backend->name()), description.c_str())
    ;

  BOOST_FOREACH(const backend_map_t::value_type &val, backends) {
    desc.add(val.second->options());
  }
}

shared_ptr<data_selection::factory> registry::create(const po::variables_map &options) {
  shared_ptr<backend> ptr = default_backend;

  if (options.count("backend")) {
    backend_map_t::iterator itr = backends.find(options["backend"].as<std::string>());
    if (itr != backends.end()) {
      ptr = itr->second;
    }
  }

  return ptr->create(options);
}

registry *registry_ptr = NULL;
boost::mutex registry_mut;
}

backend::~backend() {
}

bool register_backend(shared_ptr<backend> ptr) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->add(ptr);
}

void setup_backend_options(po::options_description &desc) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  registry_ptr->setup_options(desc);
}

shared_ptr<data_selection::factory> create_backend(const po::variables_map &options) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->create(options);
}
