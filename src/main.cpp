/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <pqxx/pqxx>
#include <iostream>

#include <boost/program_options.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <sys/wait.h>
#include <atomic>

using namespace std::chrono_literals;

#include "cgimap/logger.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/backend.hpp"
#include "cgimap/fcgi_request.hpp"
#include "cgimap/options.hpp"
#include "cgimap/process_request.hpp"
#include "cgimap/backend/apidb/apidb.hpp"


namespace po = boost::program_options;

namespace {

/**
 * global flags set by signal handlers.
 */
static std::atomic<bool> terminate_requested = false;
static std::atomic<bool> reload_requested = false;

static_assert(std::atomic<bool>::is_always_lock_free);

/**
 * SIGTERM handler.
 */
void terminate(int) {
  // termination has been requested
  terminate_requested = true;
}

/**
 * SIGHUP handler.
 */
void reload(int) {
  // reload has been requested
  reload_requested = true;
}

#if __APPLE__
  #ifndef HOST_NAME_MAX
    #define HOST_NAME_MAX 255
  #endif
#endif

/**
 * make a string to be used as the generator header
 * attribute of output files. includes some instance
 * identifying information.
 */
std::string get_generator_string() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, sizeof hostname) != 0) {
    throw std::runtime_error("gethostname returned error.");
  }

  return (fmt::format(PACKAGE_STRING " ({:d} {})", getpid(), hostname));
}

/**
 * parse the comment line and environment for options.
 */
void get_options(int argc, char **argv, po::variables_map &options) {
  po::options_description desc(PACKAGE_STRING ": Allowed options");

  // clang-format off
  desc.add_options()
    ("help", "display this help and exit")
    ("daemon", "run as a daemon")
    ("instances", po::value<int>()->default_value(5), "number of daemon instances to run")
    ("pidfile", po::value<std::string>(), "file to write pid to")
    ("logfile", po::value<std::string>(), "file to write log messages to")
    ("memcache", po::value<std::string>(), "memcache server specification")
    ("ratelimit", po::value<long>(), "average number of bytes/s to allow each client")
    ("moderator-ratelimit", po::value<long>(), "average number of bytes/s to allow each moderator")
    ("maxdebt", po::value<long>(), "maximum debt (in Mb) to allow each client before rate limiting")
    ("moderator-maxdebt", po::value<long>(), "maximum debt (in Mb) to allow each moderator before rate limiting")
    ("port", po::value<int>(), "FCGI port number (e.g. 8000) to listen on. This option is for backwards compatibility, please use --socket for new configurations.")
    ("socket", po::value<std::string>(), "FCGI port number (e.g. :8000, or 127.0.0.1:8000) or UNIX domain socket to listen on")
    ("configfile", po::value<std::string>(), "Config file")
    ;
  // clang-format on

  // add the backend options to the options description
  setup_backend_options(argc, argv, desc);

  po::options_description expert("Expert settings");

  // clang-format off
  expert.add_options()
    ("max-payload", po::value<long>(), "max size of HTTP payload allowed for uploads, after decompression (in bytes)")
    ("map-nodes", po::value<int>(), "max number of nodes allowed for /map endpoint")
    ("map-area", po::value<double>(), "max area size allowed for /map endpoint")
    ("changeset-timeout-open", po::value<std::string>(), "max open time period for a changeset")
    ("changeset-timeout-idle", po::value<std::string>(), "time period a changeset will remain open after last edit")
    ("max-changeset-elements", po::value<int>(), "max number of elements allowed in one changeset")
    ("max-way-nodes", po::value<int>(), "max number of nodes allowed in one way")
    ("scale", po::value<long>(), "conversion factor from double lat/lon to internal int format")
    ("max-relation-members", po::value<int>(), "max number of relation members per relation")
    ("max-element-tags", po::value<int>(), "max number of tags per OSM element")
    ("ratelimit-upload", po::value<bool>(), "enable rate limiting for changeset upload")
    ("bbox-size-limit-upload", po::value<bool>(), "enable bbox size limit for changeset upload")
    ;
  // clang-format on

  desc.add(expert);

  po::store(po::parse_command_line(argc, argv, desc), options);

  // Show help after parsing command line parameters
  if (options.count("help")) {
    std::cout << desc << std::endl;
    output_backend_options(std::cout);
    exit(0);
  }

  po::store(po::parse_environment(desc,
    [&desc](const std::string &name) {
          std::string option;
          // convert an environment variable name to an option name
          if (name.starts_with("CGIMAP_")) {
            std::transform(name.begin() + 7, name.end(),
                           std::back_inserter(option),
                           [](unsigned char c) {
                           return c == '_' ? '-' : std::tolower(c);
                   });
            for (const auto &d : desc.options()) {
              if (d->long_name() == option)
                return option;
            }
            std::cout << "Ignoring unknown environment variable: " << name << std::endl;
          }
          return std::string("");
        }), options);

  if (options.count("configfile")) {
    auto config_fname = options["configfile"].as<std::string>();
    std::ifstream ifs(config_fname.c_str());
    if(ifs.fail()) {
       throw std::runtime_error("Error opening config file: " + config_fname);
    }
    po::store(po::parse_config_file(ifs, desc), options);
  }

  po::notify(options);

  // for ability to accept both the old --port option in addition to socket if not available.
  if (options.count("daemon") != 0 && options.count("socket") == 0 && options.count("port") == 0) {
    throw std::runtime_error("an FCGI port number or UNIX socket is required in daemon mode");
  }
}

/**
 * loop processing fasctgi requests until are asked to stop by
 * somebody sending us a TERM signal.
 */
void process_requests(int socket, const po::variables_map &options) {
  // generator string - identifies the cgimap instance.
  auto generator = get_generator_string();
  // open any log file
  if (options.count("logfile")) {
    logger::initialise(options["logfile"].as<std::string>());
  }

  // create the rate limiter
  memcached_rate_limiter limiter(options);

  // create the routes map (from URIs to handlers)
  routes route;

  // create the request object (persists over several calls)
  fcgi_request req(socket, std::chrono::system_clock::time_point());

  // create a factory for data selections - the mechanism for actually
  // getting at data.
  auto factory = create_backend(options);
  auto update_factory = create_update_backend(options);

  logger::message("Initialised");

  // enter the main loop
  while (!terminate_requested) {
    // process any reload request
    if (reload_requested) {
      if (options.count("logfile")) {
        logger::initialise(options["logfile"].as<std::string>());
      }

      reload_requested = false;
    }

    // get the next request
    if (req.accept_r() >= 0) {
      std::chrono::system_clock::time_point now(std::chrono::system_clock::now());
      req.set_current_time(now);
      process_request(req, limiter, generator, route, *factory, update_factory.get());
    }
  }

  // finish up - dispose of the resources
  req.dispose();
}

void install_signal_handlers() {
  struct sigaction sa{};

  // install a SIGTERM handler
  sa.sa_handler = terminate;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGTERM, &sa, nullptr) < 0) {
    throw std::runtime_error("sigaction failed");
  }

  // install a SIGHUP handler
  sa.sa_handler = reload;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGHUP, &sa, nullptr) < 0) {
    throw std::runtime_error("sigaction failed");
  }
}

/**
 * make the process into a daemon by detaching from the console.
 */
void daemonise() {
  pid_t pid = 0;

  // fork to make sure we aren't a session leader
  if ((pid = fork()) < 0) {
    throw std::runtime_error("fork failed.");
  } else if (pid > 0) {
    exit(0);
  }

  // start a new session
  if (setsid() < 0) {
    throw std::runtime_error("setsid failed");
  }

  install_signal_handlers();

  // close standard descriptors
  close(0);
  close(1);
  close(2);
}


void daemon_mode(const po::variables_map &options, int socket)
{
  size_t instances = 0;

  {
    int opt_instances = options["instances"].as<int>();
    if (opt_instances > 0) {
      instances = opt_instances;
    } else {
      throw std::runtime_error(
          "Number of instances must be strictly positive.");
    }
  }

  bool children_terminated = false;
  std::set<pid_t> children;

  // make ourselves into a daemon
  daemonise();

  // record our pid if requested
  if (options.count("pidfile")) {
    std::ofstream pidfile(options["pidfile"].as<std::string>().c_str());
    pidfile << getpid() << std::endl;
  }

  // loop until we have been asked to stop and have no more children
  while (!terminate_requested || !children.empty()) {
    pid_t pid{};

    // start more children if we don't have enough
    while (!terminate_requested && (children.size() < instances)) {
      if ((pid = fork()) < 0) {
        throw std::runtime_error("fork failed.");
      } else if (pid == 0) {
        const auto start = std::chrono::steady_clock::now();
        try {
          process_requests(socket, options);
        } catch(...) {
          const auto end = std::chrono::steady_clock::now();
          const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
          if (elapsed < 1000ms) {
            std::this_thread::sleep_for(1000ms - elapsed);
          }
          throw;
        }
        exit(0);
      }

      children.insert(pid);
    }

    // wait for a child to exit
    if ((pid = wait(nullptr)) >= 0) {
      children.erase(pid);
    } else if (errno != EINTR) {
      throw std::runtime_error("wait failed.");
    }

    // pass on any termination request to our children
    if (terminate_requested && !children_terminated) {
      for (auto pid : children) { kill(pid, SIGTERM); }

      children_terminated = true;
    }

    // pass on any reload request to our children
    if (reload_requested) {
      for (auto pid : children) { kill(pid, SIGHUP); }

      reload_requested = false;
    }
  }

  // remove any pid file
  if (options.count("pidfile")) {
    remove(options["pidfile"].as<std::string>().c_str());
  }
}

void non_daemon_mode(const po::variables_map &options, int socket)
{
  if (options.count("instances") && !options["instances"].defaulted()) {
    std::cerr << "[WARN] The --instances parameter is ignored in non-daemon mode, running as single process only.\n"
                 "[WARN] If the process terminates, it must be restarted externally.\n";
  }

  install_signal_handlers();

  // record our pid if requested
  if (options.count("pidfile")) {
    std::ofstream pidfile(options["pidfile"].as<std::string>().c_str());
    pidfile << getpid() << std::endl;
  }

  // do work here
  process_requests(socket, options);

  // remove any pid file
  if (options.count("pidfile")) {
    remove(options["pidfile"].as<std::string>().c_str());
  }
}

int init_socket(const po::variables_map &options)
{
  int socket = 0;

  if (options.count("socket")) {
    if ((socket = fcgi_request::open_socket(options["socket"].as<std::string>(), 5)) < 0) {
      throw std::runtime_error("Couldn't open FCGX socket.");
    }
    // fall back to the old --port option if socket isn't available.
  } else if (options.count("port")) {
    auto sock_str = fmt::format(":{:d}", options["port"].as<int>());
    if ((socket = fcgi_request::open_socket(sock_str, 5)) < 0) {
      throw std::runtime_error("Couldn't open FCGX socket (from port).");
    }
  }
  return socket;
}

} // anonymous namespace


int main(int argc, char **argv) {
  try {
    po::variables_map options;

    // set up the apidb backend
    register_backend(make_apidb_backend());

    // get options
    get_options(argc, argv, options);

    // set global_settings based on provided options
    global_settings::set_configuration(std::make_unique<global_settings_via_options>(options));

    // get the socket to use
    auto socket = init_socket(options);

    // are we supposed to run as a daemon?
    if (options.count("daemon")) {
      daemon_mode(options, socket);
    } else {
      non_daemon_mode(options, socket);
    }
  } catch (const po::error & e) {
    std::cerr << "Error: " << e.what() << "\n(\"openstreetmap-cgimap --help\" for help)" << std::endl;
    return 1;

  } catch (const pqxx::sql_error &er) {
    logger::message(er.what());
    // Catch-all for query related postgres exceptions
    std::cerr << "Error: " << er.what() << std::endl
              << "Caused by: " << er.query() << std::endl;
    return 1;

#if PQXX_VERSION_MAJOR < 7

  } catch (const pqxx::pqxx_exception &e) {
    // Catch-all for any other postgres exceptions
    logger::message(e.base().what());
    std::cerr << "Error: " << e.base().what() << std::endl;
    return 1;

#endif

  } catch (const std::exception &e) {
    logger::message(e.what());
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
