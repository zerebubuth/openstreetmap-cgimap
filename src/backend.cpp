#include "cgimap/backend.hpp"
#include "cgimap/config.hpp"
#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <stdexcept>

namespace po = boost::program_options;
using boost::shared_ptr;

namespace {

po::variables_map first_pass_argments(int argc, char *argv[],
                                      const po::options_description &desc) {
  // copy args because boost::program_options seems to destructively consume
  // them
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  po::variables_map vm;
  // we parse the command line arguments allowing unregistered values so that
  // we can get at the --backend and --help values in order to determine which
  // backend's options should be added to the description for the proper
  // parsing phase. bit of a hack, but couldn't figure out a better way to do
  // it.
  po::store(
      po::command_line_parser(args).options(desc).allow_unregistered().run(),
      vm);
  po::notify(vm);
  return vm;
}

struct registry {
  registry();

  bool add(shared_ptr<backend> ptr);
  void setup_options(int argc, char *argv[], po::options_description &desc);
  void output_options(std::ostream &out);
  shared_ptr<data_selection::factory> create(const po::variables_map &options);
  shared_ptr<oauth::store> create_oauth_store(const boost::program_options::variables_map &opts);

private:
  typedef std::map<std::string, shared_ptr<backend> > backend_map_t;
  backend_map_t backends;
  shared_ptr<backend> default_backend;
};

registry::registry() {}

bool registry::add(shared_ptr<backend> ptr) {
  if (default_backend) {
    if (ptr->name() <= default_backend->name()) {
      default_backend = ptr;
    }
  } else {
    default_backend = ptr;
  }

  backends[ptr->name()] = ptr;

  return true;
}

void registry::setup_options(int argc, char *argv[],
                             po::options_description &desc) {
  if (backends.empty() || !default_backend) {
    throw std::runtime_error("No backends available - this is most likely a "
                             "compile-time configuration error.");
  }

  std::ostringstream all_backends;
  bool first = true;
  BOOST_FOREACH(const backend_map_t::value_type &val, backends) {
    if (first) {
      first = false;
    } else {
      all_backends << ", ";
    }
    all_backends << val.second->name();
  }

  std::string description =
      (boost::format("backend to use, available options are: %1%") %
       all_backends.str()).str();
  desc.add_options()("backend", po::value<std::string>()->default_value(
                                    default_backend->name()),
                     description.c_str());

  po::variables_map vm = first_pass_argments(argc, argv, desc);

  // little hack - we want to print *all* the backends when --help is passed, so
  // we don't add one here when it's present. it's a nasty way to do it, but i
  // can't think of a better one right now...
  if (!vm.count("help")) {
    shared_ptr<backend> ptr = default_backend;

    if (vm.count("backend")) {
      backend_map_t::iterator itr =
          backends.find(vm["backend"].as<std::string>());
      if (itr != backends.end()) {
        ptr = itr->second;
      }
    }

    desc.add(ptr->options());
  }
}

void registry::output_options(std::ostream &out) {
  BOOST_FOREACH(const backend_map_t::value_type &val, backends) {
    out << val.second->options() << std::endl;
  }
}

shared_ptr<data_selection::factory>
registry::create(const po::variables_map &options) {
  shared_ptr<backend> ptr = default_backend;

  if (options.count("backend")) {
    backend_map_t::iterator itr =
        backends.find(options["backend"].as<std::string>());
    if (itr != backends.end()) {
      ptr = itr->second;
    }
  }

  return ptr->create(options);
}

boost::shared_ptr<oauth::store>
registry::create_oauth_store(const boost::program_options::variables_map &options) {
  shared_ptr<backend> ptr = default_backend;

  if (options.count("backend")) {
    backend_map_t::iterator itr =
        backends.find(options["backend"].as<std::string>());
    if (itr != backends.end()) {
      ptr = itr->second;
    }
  }

  return ptr->create_oauth_store(options);
}

registry *registry_ptr = NULL;
boost::mutex registry_mut;

} // anonymous namespace

backend::~backend() {}

bool register_backend(shared_ptr<backend> ptr) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->add(ptr);
}

void setup_backend_options(int argc, char *argv[],
                           po::options_description &desc) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  registry_ptr->setup_options(argc, argv, desc);
}

void output_backend_options(std::ostream &out) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  registry_ptr->output_options(out);
}

shared_ptr<data_selection::factory>
create_backend(const po::variables_map &options) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->create(options);
}

boost::shared_ptr<oauth::store>
create_oauth_store(const po::variables_map &options) {
  boost::unique_lock<boost::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->create_oauth_store(options);
}
