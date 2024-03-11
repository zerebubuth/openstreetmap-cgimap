/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend.hpp"

#include <fmt/core.h>
#include <stdexcept>
#include <mutex>

namespace po = boost::program_options;


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
  registry() = default;

  bool add(std::unique_ptr<backend> ptr);
  void setup_options(int argc, char *argv[], po::options_description &desc);
  void output_options(std::ostream &out);
  std::unique_ptr<data_selection::factory> create(const po::variables_map &options);
  std::unique_ptr<data_update::factory> create_data_update(const po::variables_map &options);
  std::unique_ptr<oauth::store> create_oauth_store(const boost::program_options::variables_map &opts);

private:
  using backend_map_t = std::map<std::string, std::unique_ptr<backend> >;
  backend_map_t backends;
  std::optional<std::string> default_backend;
};

bool registry::add(std::unique_ptr<backend> ptr) {
  if (default_backend) {
    if (ptr->name() <= *default_backend) {
      default_backend = ptr->name();
    }
  } else {
    default_backend = ptr->name();
  }

  std::string name = ptr->name();
  backends.emplace(name, std::move(ptr));

  return true;
}

void registry::setup_options(int argc, char *argv[],
                             po::options_description &desc) {
  if (backends.empty() || (!(default_backend))) {
    throw std::runtime_error("No backends available - this is most likely a "
                             "compile-time configuration error.");
  }

  std::string all_backends;

  for (const backend_map_t::value_type &val : backends) {
    if (!all_backends.empty()) {
      all_backends += ", ";
    }
    all_backends += val.second->name();
  }

  std::string description = fmt::format("backend to use, available options are: {}", all_backends);

  desc.add_options()("backend", po::value<std::string>()->default_value(*default_backend),
                     description.c_str());

  po::variables_map vm = first_pass_argments(argc, argv, desc);

  std::string bcknd = *default_backend;

  // little hack - we want to print *all* the backends when --help is passed, so
  // we don't add one here when it's present. it's a nasty way to do it, but i
  // can't think of a better one right now...
  if (!vm.count("help")) {

    if (vm.count("backend")) {
      auto itr =
          backends.find(vm["backend"].as<std::string>());
      if (itr != backends.end()) {
        bcknd = itr->first;
      }
      else {
        throw std::runtime_error(fmt::format("unknown backend provided, available options are: {}", all_backends));
      }
    }

    desc.add(backends[bcknd]->options());
  }
}

void registry::output_options(std::ostream &out) {
  for (const backend_map_t::value_type &val : backends) {
    out << val.second->options() << std::endl;
  }
}

std::unique_ptr<data_selection::factory>
registry::create(const po::variables_map &options) {
  std::string bcknd = *default_backend;

  if (options.count("backend")) {
    auto itr =
        backends.find(options["backend"].as<std::string>());
    if (itr != backends.end()) {
      bcknd = itr->first;
    }
  }

  return backends[bcknd]->create(options);
}

std::unique_ptr<data_update::factory>
registry::create_data_update(const po::variables_map &options) {
  std::string bcknd = *default_backend;

  if (options.count("backend")) {
    auto itr =
        backends.find(options["backend"].as<std::string>());
    if (itr != backends.end()) {
      bcknd = itr->first;
    }
  }

  return backends[bcknd]->create_data_update(options);
}


std::unique_ptr<oauth::store>
registry::create_oauth_store(const boost::program_options::variables_map &options) {
  std::string bcknd = *default_backend;

  if (options.count("backend")) {
    auto itr =
        backends.find(options["backend"].as<std::string>());
    if (itr != backends.end()) {
      bcknd = itr->first;
    }
  }

  return backends[bcknd]->create_oauth_store(options);
}

registry *registry_ptr = NULL;
std::mutex registry_mut;

} // anonymous namespace

backend::~backend() = default;

bool register_backend(std::unique_ptr<backend> ptr) {
  std::unique_lock<std::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->add(std::move(ptr));
}

void setup_backend_options(int argc, char *argv[],
                           po::options_description &desc) {
  std::unique_lock<std::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  registry_ptr->setup_options(argc, argv, desc);
}

void output_backend_options(std::ostream &out) {
  std::unique_lock<std::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  registry_ptr->output_options(out);
}

std::unique_ptr<data_selection::factory>
create_backend(const po::variables_map &options) {
  std::unique_lock<std::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->create(options);
}

std::unique_ptr<data_update::factory>
create_update_backend(const po::variables_map &options) {
  std::unique_lock<std::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->create_data_update(options);
}

std::unique_ptr<oauth::store>
create_oauth_store(const po::variables_map &options) {
  std::unique_lock<std::mutex> lock(registry_mut);
  if (registry_ptr == NULL) {
    registry_ptr = new registry;
  }

  return registry_ptr->create_oauth_store(options);
}
