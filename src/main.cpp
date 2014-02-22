#include <pqxx/pqxx>
#include <iostream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/tuple/tuple.hpp>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <fcgiapp.h>
#include <memory>
#include <algorithm>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include "bbox.hpp"
#include "http.hpp"
#include "logger.hpp"
#include "output_formatter.hpp"
#include "output_writer.hpp"
#include "handler.hpp"
#include "routes.hpp"
#include "fcgi_helpers.hpp"
#include "rate_limiter.hpp"
#include "choose_formatter.hpp"
#include "backend.hpp"
#include "config.h"

using std::runtime_error;
using std::vector;
using std::string;
using std::map;
using std::ostringstream;
using std::auto_ptr;
using boost::shared_ptr;
using boost::format;

namespace al = boost::algorithm;
namespace pt = boost::posix_time;
namespace po = boost::program_options;

/**
 * global flags set by signal handlers.
 */
static bool terminate_requested = false;
static bool reload_requested = false;

/**
 * get encoding to use for response.
 */
shared_ptr<http::encoding>
get_encoding(FCGX_Request &req) {
  const char *accept_encoding = FCGX_GetParam("HTTP_ACCEPT_ENCODING", req.envp);

  if (accept_encoding) {
     return http::choose_encoding(string(accept_encoding));
  }
  else {
     return shared_ptr<http::identity>(new http::identity());
  }
}

/**
 * get CORS access control headers to include in response.
 */
string
get_cors_headers(FCGX_Request &req) {
  const char *origin = FCGX_GetParam("HTTP_ORIGIN", req.envp);
  ostringstream headers;

  if (origin) {
     headers << "Access-Control-Allow-Credentials: true\r\n";
     headers << "Access-Control-Allow-Methods: GET\r\n";
     headers << "Access-Control-Allow-Origin: " << origin << "\r\n";
     headers << "Access-Control-Max-Age: 1728000\r\n";
  }

  return headers.str();
}


void
respond_error(const http::exception &e, FCGX_Request &r) {
  logger::message(format("Returning with http error %1% with reason %2%") % e.code() %e.what());

  const char *error_format = FCGX_GetParam("HTTP_X_ERROR_FORMAT", r.envp);
  string cors_headers = get_cors_headers(r);

  ostringstream ostr;
  if (error_format && al::iequals(error_format, "xml")) {
    ostr << "Status: 200 OK\r\n"
         << "Content-Type: text/xml; charset=utf-8\r\n"
         << cors_headers
         << "\r\n"
         << "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n"
         << "<osmError>\r\n"
         << "<status>" << e.code() << " " << e.header() << "</status>\r\n"
         << "<message>" << e.what() << "</message>\r\n"
         << "</osmError>\r\n";
  } else {
    ostr << "Status: " << e.code() << " " << e.header() << "\r\n"
         << "Content-Type: text/html\r\n"
         << "Content-Length: 0\r\n"
         << "Error: " << e.what() << "\r\n"
         << "Cache-Control: no-cache\r\n"
         << cors_headers
         << "\r\n";
  }

  FCGX_PutS(ostr.str().c_str(), r.out);
}

bool
get_env(const char *k, string &s) {
  char *v = getenv(k);
  return v == NULL ? false : (s = v, true);
}

/**
 * Bindings to allow libxml to write directly to the FCGI
 * library.
 */
class fcgi_output_buffer
  : public output_buffer {
public:
  virtual int write(const char *buffer, int len) {
    w += len;
    return FCGX_PutStr(buffer, len, r.out);
  }

  virtual int close() {
    // we don't actually close the FCGI output, as that happens
    // automatically on the next call to accept.
    return 0;
  }

  virtual int written() {
    return w;
  }

  virtual void flush() {
    // there's a note that says this causes too many writes and decreases 
    // efficiency, but we're only calling it once...
    FCGX_FFlush(r.out);
  }

  virtual ~fcgi_output_buffer() {
  }

  fcgi_output_buffer(FCGX_Request &req) 
    : r(req), w(0) {
  }

private:
  FCGX_Request &r;
  int w;
};

/**
 * parse the comment line and environment for options.
 */
static void
get_options(int argc, char **argv, po::variables_map &options) {
  po::options_description desc(PACKAGE_STRING ": Allowed options");

  desc.add_options()
    ("help", "display this help and exit")
    ("daemon", "run as a daemon")
    ("instances", po::value<int>()->default_value(5), "number of daemon instances to run")
    ("pidfile", po::value<string>(), "file to write pid to")
    ("logfile", po::value<string>(), "file to write log messages to")
    ("memcache", po::value<string>(), "memcache server specification")
    ("ratelimit", po::value<int>(), "average number of bytes/s to allow each client")
    ("maxdebt", po::value<int>(), "maximum debt (in Mb) to allow each client before rate limiting")
    ("port", po::value<int>(), "FCGI port number to listen on")
    ;

  // add the backend options to the options description
  setup_backend_options(argc, argv, desc);

  po::store(po::parse_command_line(argc, argv, desc), options);
  po::store(po::parse_environment(desc, "CGIMAP_"), options);
  po::notify(options);

  if (options.count("help")) {
    std::cout << desc << std::endl;
    output_backend_options(std::cout);
    exit(1);
  }

  if (options.count("daemon") != 0 && options.count("port") == 0) {
    throw runtime_error("an FCGI port number is required in daemon mode");
  }
}

/**
 * Return a 405 error.
 */
void
process_not_allowed(FCGX_Request &request) {
  FCGX_FPrintF(request.out,
               "Status: 405 Method Not Allowed\r\n"
               "Allow: GET, HEAD, OPTIONS\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: 0\r\n"
               "Cache-Control: no-cache\r\n\r\n");
}

/**
 * process a GET request.
 */
boost::tuple<string, size_t>
process_get_request(FCGX_Request &request, routes &route, 
                    boost::shared_ptr<data_selection::factory> factory,
		    const string &ip, const string &generator) {
  // figure how to handle the request
  handler_ptr_t handler = route(request);
  
  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started request for %1% from %2%") % request_name % ip);
  
  // separate transaction for the request
  shared_ptr<data_selection> selection = factory->make_selection();
  
  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler->responder(*selection);
  
  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(request);
  
  // create the XML writer with the FCGI streams as output
  shared_ptr<output_buffer> out =
    shared_ptr<fcgi_output_buffer>(new fcgi_output_buffer(request));
  out = encoding->buffer(out);
  
  // create the correct mime type output formatter.
  shared_ptr<output_formatter> o_formatter = choose_formatter(request, responder, out);
  
  // get any CORS headers to return
  string cors_headers = get_cors_headers(request);
  
  // TODO: use handler/responder to setup response headers.
  // write the response header
  FCGX_FPrintF(request.out,
	       "Status: 200 OK\r\n"
	       "Content-Type: %s; charset=utf-8\r\n"
               "%s"
	       "Content-Encoding: %s\r\n"
	       "Cache-Control: no-cache\r\n"
	       "%s"
	       "\r\n", 
               mime::to_string(o_formatter->mime_type()).c_str(),
               responder->extra_response_headers().c_str(),
               encoding->name().c_str(), cors_headers.c_str());
  
  try {
    // call to write the response
    responder->write(o_formatter, generator);
    
    // make sure all bytes have been written. note that the writer can
    // throw an exception here, leaving the xml document in a 
    // half-written state...
    o_formatter->flush();
    out->flush();
    
  } catch (const output_writer::write_error &e) {
    // don't do anything - just go on to the next request.
    logger::message(format("Caught write error, aborting request: %1%") % e.what());
    
  } catch (const std::exception &e) {
    // errors here are unrecoverable (fatal to the request but maybe
    // not fatal to the process) since we already started writing to
    // the client.
    o_formatter->error(e.what());
  }

  return boost::make_tuple(request_name, out->written());
}

/**
 * process a HEAD request.
 */
boost::tuple<string, size_t>
process_head_request(FCGX_Request &request, routes &route,
                    boost::shared_ptr<data_selection::factory> factory,
		    const string &ip) {
  // figure how to handle the request
  handler_ptr_t handler = route(request);

  // request start logging
  string request_name = handler->log_name();
  logger::message(format("Started HEAD request for %1% from %2%") % request_name % ip);

  // We don't actually use the resulting data from the DB request,
  // but it might throw an error which results in a 404 or 410 response

  // The 404 and 410 responses have an empty message-body so we're safe using them unmodified

  // separate transaction for the request
  shared_ptr<data_selection> selection = factory->make_selection();

  // constructor of responder handles dynamic validation (i.e: with db access).
  responder_ptr_t responder = handler->responder(*selection);

  // get encoding to use
  shared_ptr<http::encoding> encoding = get_encoding(request);

  // get any CORS headers to return
  string cors_headers = get_cors_headers(request);

  // TODO: use handler/responder to setup response headers.
  // write the response header
  FCGX_FPrintF(request.out,
	       "Status: 200 OK\r\n"
	       "Content-Type: text/xml; charset=utf-8\r\n"
               "%s"
	       "Content-Encoding: %s\r\n"
	       "Cache-Control: no-cache\r\n"
	       "%s"
	       "\r\n", 
               responder->extra_response_headers().c_str(),
               encoding->name().c_str(), cors_headers.c_str());

  return boost::make_tuple(request_name, 0);
}

/**
 * process an OPTIONS request.
 */
boost::tuple<string, size_t>
process_options_request(FCGX_Request &request) {
  static const string request_name = "OPTIONS";
  const char *origin = FCGX_GetParam("HTTP_ORIGIN", request.envp);
  const char *method = FCGX_GetParam("HTTP_ACCESS_CONTROL_REQUEST_METHOD", request.envp);

  if (origin && (strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {
    // get the CORS headers to return
    string cors_headers = get_cors_headers(request);

    // write the response
    FCGX_FPrintF(request.out,
                 "Status: 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "%s"
                 "\r\n\r\n", cors_headers.c_str());
  } else {
    process_not_allowed(request);
  }
  return boost::make_tuple(request_name, 0);
}

/**
 * make a string to be used as the generator header
 * attribute of output files. includes some instance
 * identifying information.
 */
string get_generator_string() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, sizeof hostname) != 0) {
    throw std::runtime_error("gethostname returned error.");
  }
  
  return (boost::format(PACKAGE_STRING " (%1% %2%)") 
          % getpid() % hostname).str();
}

/**
 * loop processing fasctgi requests until are asked to stop by
 * somebody sending us a TERM signal.
 */
static void
process_requests(int socket, const po::variables_map &options) {
  // generator string - identifies the cgimap instance.
  string generator = get_generator_string();
  // open any log file
  if (options.count("logfile")) {
    logger::initialise(options["logfile"].as<string>());
  }

  // initialise FCGI
  if (FCGX_Init() != 0) {
    throw runtime_error("Couldn't initialise FCGX library.");
  }

  // create the rate limiter
  rate_limiter limiter(options);

  // create the routes map (from URIs to handlers)
  routes route;

  // create the request object for fcgi calls
  FCGX_Request request;
  if (FCGX_InitRequest(&request, socket, FCGI_FAIL_ACCEPT_ON_INTR) != 0) {
    throw runtime_error("Couldn't initialise FCGX request structure.");
  }

  // create a factory for data selections - the mechanism for actually
  // getting at data.
  boost::shared_ptr<data_selection::factory> factory = create_backend(options);

  logger::message("Initialised");

  // enter the main loop
  while (!terminate_requested) {
    // process any reload request
    if (reload_requested) {
      if (options.count("logfile")) {
	logger::initialise(options["logfile"].as<string>());
      }

      reload_requested = false;
    }

    // get the next request
    if (FCGX_Accept_r(&request) >= 0)
    {
      try {
	// get the client IP address
	string ip = fcgi_get_env(request, "REMOTE_ADDR");
	pt::ptime start_time(pt::second_clock::local_time());
	
	// check whether the client is being rate limited
	if (!limiter.check(ip)) {
	  logger::message(format("Rate limiter rejected request from %1%") % ip);
	  throw http::bandwidth_limit_exceeded("You have downloaded too much "
					       "data. Please try again later.");
	}

        // get the request method
        string method = fcgi_get_env(request, "REQUEST_METHOD");

	// data returned from request methods
	string request_name;
	size_t bytes_written;

        // process request
        if (method == "GET") {
          boost::tie(request_name, bytes_written) = process_get_request(request, route, factory, ip, generator);
        } else if (method == "HEAD") {
          boost::tie(request_name, bytes_written) = process_head_request(request, route, factory, ip);
        } else if (method == "OPTIONS") {
          boost::tie(request_name, bytes_written) = process_options_request(request);
        } else {
          process_not_allowed(request);
        }

	// update the rate limiter, if anything was written
	if (bytes_written > 0) {
	  limiter.update(ip, bytes_written);
	}
	
	// log the completion time (note: this comes last to avoid
	// logging twice when an error is thrown.)
	pt::ptime end_time(pt::second_clock::local_time());
	logger::message(format("Completed request for %1% from %2% in %3% ms returning %4% bytes") 
			% request_name % ip % (end_time - start_time).total_milliseconds() % bytes_written);

      } catch (const http::exception &e) {
	// errors here occur before we've started writing the response
	// so we can send something helpful back to the client.
	respond_error(e, request);

      } catch (const std::exception &e) {
	// catch an error here to provide feedback to the user
	respond_error(http::server_error(e.what()), request);
	
	// re-throw the exception for higher-level handling
	throw;
      }

    } else if (errno != EINTR) {
       char err_buf[1024];
       std::ostringstream out;

       if (errno == ENOTSOCK) {
          out << "FCGI port not set properly, please use the --port option "
              << "(caused by ENOTSOCK).";

       } else {
          out << "error accepting request: ";
          if (strerror_r(errno, err_buf, sizeof err_buf) == 0) {
             out << err_buf;
          } else {
             out << "error encountered while getting error message";
          }
       }

       throw runtime_error(out.str());
    }
  }

  // finish up
  FCGX_Finish_r(&request);
  FCGX_Free(&request, true);
}

/**
 * SIGTERM handler.
 */
static void
terminate(int signum) {
  // termination has been requested
  terminate_requested = true;
}

/**
 * SIGHUP handler.
 */
static void
reload(int signum) {
  // reload has been requested
  reload_requested = true;
}

/**
 * make the process into a daemon by detaching from the console.
 */
static void
daemonise(void) {
  pid_t pid;
  struct sigaction sa;

  // fork to make sure we aren't a session leader
  if ((pid = fork()) < 0) {
    throw runtime_error("fork failed.");
  } else if (pid > 0) {
    exit(0);
  }

  // start a new session
  if (setsid() < 0) {
    throw runtime_error("setsid failed");
  }

  // install a SIGTERM handler
  sa.sa_handler = terminate;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGTERM, &sa, NULL) < 0) {
    throw runtime_error("sigaction failed");
  }

  // install a SIGHUP handler
  sa.sa_handler = reload;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGHUP, &sa, NULL) < 0) {
    throw runtime_error("sigaction failed");
  }

  // close standard descriptors
  close(0);
  close(1);
  close(2);
}

int
main(int argc, char **argv) {
  try {
    po::variables_map options;
    int socket;

    // get options
    get_options(argc, argv, options);

    // get the socket to use
    if (options.count("port")) {
      ostringstream path;

      path << ":" << options["port"].as<int>();

      if ((socket = FCGX_OpenSocket(path.str().c_str(), 5)) < 0) {
	throw runtime_error("Couldn't open FCGX socket.");
      }
    } else {
      socket = 0;
    }

    // are we supposed to run as a daemon?
    if (options.count("daemon")) {
      size_t instances = 0;
      {
        int opt_instances = options["instances"].as<int>();
        if (opt_instances > 0) {
           instances = opt_instances;
        } else {
           throw std::runtime_error("Number of instances must be strictly positive.");
        }
      }
      
      bool children_terminated = false;
      std::set<pid_t> children;

      // make ourselves into a daemon
      daemonise();

      // record our pid if requested
      if (options.count("pidfile")) {
	std::ofstream pidfile(options["pidfile"].as<string>().c_str());
	pidfile << getpid() << std::endl;
      }

      // loop until we have been asked to stop and have no more children
      while (!terminate_requested || children.size() > 0) {
	pid_t pid;

	// start more children if we don't have enough
	while (!terminate_requested && (children.size() < instances)) {
	  if ((pid = fork()) < 0)
	  {
	    throw runtime_error("fork failed.");
	  }
	  else if (pid == 0)
	  {
	    process_requests(socket, options);
	    exit(0);
	  }

	  children.insert(pid);
	}

	// wait for a child to exit
	if ((pid = wait(NULL)) >= 0) {
	  children.erase(pid);
	} else if (errno != EINTR) {
	  throw runtime_error("wait failed.");
	}

	// pass on any termination request to our children
	if (terminate_requested && !children_terminated) {
	  BOOST_FOREACH(pid, children) {
	    kill(pid, SIGTERM);
	  }

	  children_terminated = true;
	}

	// pass on any reload request to our children
	if (reload_requested) {
	  BOOST_FOREACH(pid, children) {
	    kill(pid, SIGHUP);
	  }

	  reload_requested = false;
	}
      }

      // remove any pid file
      if (options.count("pidfile")) {
	remove(options["pidfile"].as<string>().c_str());
      }
    }
    else
    {
      // record our pid if requested
      if (options.count("pidfile")) {
	std::ofstream pidfile(options["pidfile"].as<string>().c_str());
	pidfile << getpid() << std::endl;
      }

      // do work here
      process_requests(socket, options);

      // remove any pid file
      if (options.count("pidfile")) {
	remove(options["pidfile"].as<string>().c_str());
      }
    }
  } catch (const pqxx::sql_error &er) {
    // Catch-all for query related postgres exceptions
    std::cerr << "Error: " << er.what() << std::endl
	      << "Caused by: " << er.query() << std::endl;
    return 1;

  } catch (const pqxx::pqxx_exception &e) {
    // Catch-all for any other postgres exceptions
    std::cerr << "Error: " << e.base().what() << std::endl;
    return 1;

  } catch (const std::exception &e) {
    logger::message(e.what());
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;

  }

  return 0;
}
