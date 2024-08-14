/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend.hpp"

#include <fmt/core.h>
#include <memory>

namespace po = boost::program_options;


namespace {

po::variables_map first_pass_arguments(int argc, char *argv[],
                                      const po::options_description &desc) {
  // copy args because boost::program_options seems to destructively consume
  // them
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
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

  bool set_backend(std::unique_ptr<backend> ptr);
  void setup_options(int argc, char *argv[], po::options_description &desc);
  void output_options(std::ostream &out);
  std::unique_ptr<data_selection::factory> create(const po::variables_map &options);
  std::unique_ptr<data_update::factory> create_data_update(const po::variables_map &options);

private:
  std::unique_ptr<backend> backend_ptr;
};

bool registry::set_backend(std::unique_ptr<backend> ptr) {
  backend_ptr = std::move(ptr);
  return true;
}

void registry::setup_options(int argc, char *argv[],
                             po::options_description &desc) {

  po::variables_map vm = first_pass_arguments(argc, argv, desc);

  if (!vm.count("help")) {
    desc.add(backend_ptr->options());
  }
}

void registry::output_options(std::ostream &out) {
    out << backend_ptr->options() << std::endl;
}

std::unique_ptr<data_selection::factory>
registry::create(const po::variables_map &options) {
  return backend_ptr->create(options);
}

std::unique_ptr<data_update::factory>
registry::create_data_update(const po::variables_map &options) {
  return backend_ptr->create_data_update(options);
}

std::unique_ptr<registry> registry_ptr = std::make_unique<registry>();


} // anonymous namespace

backend::~backend() = default;

// Registers a single backend, replacing an existing backend
bool register_backend(std::unique_ptr<backend> ptr) {
  return registry_ptr->set_backend(std::move(ptr));
}

void setup_backend_options(int argc, char *argv[],
                           po::options_description &desc) {

  registry_ptr->setup_options(argc, argv, desc);
}

void output_backend_options(std::ostream &out) {
  registry_ptr->output_options(out);
}

std::unique_ptr<data_selection::factory>
create_backend(const po::variables_map &options) {
  return registry_ptr->create(options);
}

std::unique_ptr<data_update::factory>
create_update_backend(const po::variables_map &options) {
  return registry_ptr->create_data_update(options);
}

