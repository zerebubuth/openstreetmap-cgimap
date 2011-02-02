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
#include <cmath>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <fcgiapp.h>
#include <memory>
#include <algorithm>
#include <errno.h>
#include <sys/wait.h>

#include "bbox.hpp"
#include "temp_tables.hpp"
#include "writer.hpp"
#include "split_tags.hpp"
#include "map.hpp"
#include "http.hpp"
#include "logger.hpp"
#include "rate_limiter.hpp"

using std::runtime_error;
using std::vector;
using std::string;
using std::map;
using std::ostringstream;
using std::auto_ptr;
using boost::shared_ptr;
using boost::format;

namespace pt = boost::posix_time;
namespace po = boost::program_options;

#define MAX_AREA 0.25
#define CACHE_SIZE 1000

/**
 * global flags set by signal handlers.
 */
static bool terminate_requested = false;
static bool reload_requested = false;

/**
 * Lookup a string from the FCGI environment. Throws 500 error if the
 * string isn't there.
 */
string
fcgi_get_env(FCGX_Request &req, const char* name) {
  assert(name);
  const char* v = FCGX_GetParam(name, req.envp);

  // since the map script is so simple i'm just going to assume that
  // any time we fail to get an environment variable is a fatal error.
  if (v == NULL) {
    ostringstream ostr;
    ostr << "FCGI didn't set the $" << name << " environment variable.";
    throw http::server_error(ostr.str());
  }

  return string(v);
}

/**
 * get a query string by hook or by crook.
 *
 * the $QUERY_STRING variable is supposed to be set, but it isn't if 
 * cgimap is invoked on the 404 path, which seems to be a pretty common
 * case for doing routing/queueing in lighttpd. in that case, try and
 * parse the $REQUEST_URI.
 */
string
get_query_string(FCGX_Request &req) {
  // try the query string that's supposed to be present first
  const char *query_string = FCGX_GetParam("QUERY_STRING", req.envp);
  
  // if that isn't present, then this may be being invoked as part of a
  // 404 handler, so look at the request uri instead.
  if ((query_string == NULL) || (strlen(query_string) == 0)) {
    const char *request_uri = FCGX_GetParam("REQUEST_URI", req.envp);

    if ((request_uri == NULL) || (strlen(request_uri) == 0)) {
      // fail. something has obviously gone massively wrong.
      ostringstream ostr;
      ostr << "FCGI didn't set the $QUERY_STRING or $REQUEST_URI "
	   << "environment variables.";
      throw http::server_error(ostr.str());
    }

    const char *request_uri_end = request_uri + strlen(request_uri);
    // i think the only valid position for the '?' char is at the beginning
    // of the query string.
    const char *question_mark = std::find(request_uri, request_uri_end, '?');
    if (question_mark == request_uri_end) {
      return string();
    } else {
      return string(question_mark + 1);
    }

  } else {
    return string(query_string);
  }
}

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
 * Validates an FCGI request, returning the valid bounding box or 
 * throwing an error if there was no valid bounding box.
 */
bbox
validate_request(FCGX_Request &request) {
  // check that the REQUEST_METHOD is a GET
  if (fcgi_get_env(request, "REQUEST_METHOD") != "GET") 
    throw http::method_not_allowed("Only the GET method is supported for "
				   "map requests.");

  string decoded = http::urldecode(get_query_string(request));
  const map<string, string> params = http::parse_params(decoded);
  map<string, string>::const_iterator itr = params.find("bbox");

  bbox bounds;
  if ((itr == params.end()) ||
      !bounds.parse(itr->second)) {
    throw http::bad_request("The parameter bbox is required, and must be "
			    "of the form min_lon,min_lat,max_lon,max_lat.");
  }

  // clip the bounding box against the world
  bounds.clip_to_world();

  // check that the bounding box is within acceptable limits. these
  // limits taken straight from the ruby map implementation.
  if (!bounds.valid()) {
    throw http::bad_request("The latitudes must be between -90 and 90, "
			    "longitudes between -180 and 180 and the "
			    "minima must be less than the maxima.");
  }

  if (bounds.area() > MAX_AREA) {
    ostringstream ostr;
    ostr << "The maximum bbox size is " << MAX_AREA << ", and your request "
      "was too large. Either request a smaller area, or use planet.osm";
    throw http::bad_request(ostr.str());
  }

  return bounds;
}

void
respond_error(const http::exception &e, FCGX_Request &r) {
  ostringstream ostr;
  ostr << "Status: " << e.code() << " " << e.header() << "\r\n"
       << "Content-Type: text/html\r\n"
       << "Error: " << e.what() << "\r\n"
       << "\r\n"
       << "<html><head><title>" << e.header() << "</title></head>"
       << "<body><p>" << e.what() << "</p></body></html>\n";
  
  FCGX_PutS(ostr.str().c_str(), r.out);
}

bool
get_env(const char *k, string &s) {
  char *v = getenv(k);
  return v == NULL ? false : (s = v, true);
}

auto_ptr<pqxx::connection>
connect_db(const po::variables_map &options) {
  // build the connection string.
  ostringstream ostr;
  ostr << "dbname=" << options["dbname"].as<std::string>();
  if (options.count("host")) {
    ostr << " host=" << options["host"].as<std::string>();
  }
  if (options.count("username")) {
    ostr << " user=" << options["username"].as<std::string>();
  }
  if (options.count("password")) {
    ostr << " password=" << options["password"].as<std::string>();
  }

  // connect to the database.
  auto_ptr<pqxx::connection> con(new pqxx::connection(ostr.str()));

  // set the connections to use the appropriate charset.
  con->set_client_encoding(options["charset"].as<std::string>());

  return con;
}

/**
 * Bindings to allow libxml to write directly to the FCGI
 * library.
 */
class fcgi_output_buffer
  : public xml_writer::output_buffer {
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
  po::options_description desc("Allowed options");

  desc.add_options()
    ("help", "display this help and exit")
    ("dbname", po::value<string>(), "database name")
    ("host", po::value<string>(), "database server host")
    ("username", po::value<string>(), "database user name")
    ("password", po::value<string>(), "database password")
    ("charset", po::value<string>()->default_value("utf8"), "database character set")
    ("port", po::value<int>(), "port number to use")
    ("daemon", "run as a daemon")
    ("instances", po::value<int>()->default_value(5), "number of daemon instances to run")
    ("pidfile", po::value<string>(), "file to write pid to")
    ("logfile", po::value<string>(), "file to write log messages to")
    ("memcache", po::value<string>(), "memcache server specification")
    ("ratelimit", po::value<int>(), "average number of bytes/s to allow each client")
    ("maxdebt", po::value<int>(), "maximum debt (in Mb) to allow each client before rate limiting");

  po::store(po::parse_command_line(argc, argv, desc), options);
  po::store(po::parse_environment(desc, "CGIMAP_"), options);
  po::notify(options);

  if (options.count("help")) {
    std::cout << desc << std::endl;
    exit(1);
  }

  if (options.count("dbname") == 0) {
    throw runtime_error("database name not specified");
  }

  if (options.count("daemon") != 0 && options.count("port") == 0) {
    throw runtime_error("a port number is required in daemon mode");
  }
}

/**
 * loop processing fasctgi requests until are asked to stop by
 * somebody sending us a TERM signal.
 */
static void
process_requests(int socket, const po::variables_map &options) {
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

  // get the parameters for the connection from the environment
  // and connect to the database, throws exceptions if it fails.
  auto_ptr<pqxx::connection> con = connect_db(options);
  auto_ptr<pqxx::connection> cache_con = connect_db(options);

  // create the request object for fcgi calls
  FCGX_Request request;
  if (FCGX_InitRequest(&request, socket, FCGI_FAIL_ACCEPT_ON_INTR) != 0) {
    throw runtime_error("Couldn't initialise FCGX request structure.");
  }

  // start a transaction using a second connection just for looking up 
  // users/changesets for the cache.
  pqxx::nontransaction cache_x(*cache_con, "changeset_cache");
  cache<long int, changeset> changeset_cache(boost::bind(fetch_changeset, boost::ref(cache_x), _1), CACHE_SIZE);

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
	// read all the input data..?

        // get the client IP address
        string ip = fcgi_get_env(request, "REMOTE_ADDR");

        // check whether the client is being rate limited
        if (!limiter.check(ip)) {
          logger::message(format("Rate limiter rejected request from %1%") % ip);
          throw http::bandwidth_limit_exceeded("You have downloaded too much "
                                               "data. Please try again later.");
        }

	// validate the input
	bbox bounds = validate_request(request);

        /// log the start of the request
	pt::ptime start_time(pt::second_clock::local_time());
	logger::message(format("Started request for %1%,%2%,%3%,%4% from %5%") % bounds.minlon % bounds.minlat % bounds.maxlon % bounds.maxlat % ip);

	// separate transaction for the request
	pqxx::work x(*con);

	// create temporary tables of nodes, ways and relations which
	// are in or used by elements in the bbox
	tmp_nodes tn(x, bounds);

	// check how many nodes we got
	pqxx::result res = x.exec("select count(*) from tmp_nodes");
	int num_nodes = res[0][0].as<int>();
	if (num_nodes > 50000) {
	  throw http::bad_request("You requested too many nodes (limit is "
				  "50000). Either request a smaller area, "
				  "or use planet.osm");
	}

	tmp_ways tw(x);
	tmp_relations tr(x);

	// get encoding to use
	shared_ptr<http::encoding> encoding = get_encoding(request);

	// write the response header
	FCGX_FPrintF(request.out,
		     "Status: 200 OK\r\n"
		     "Content-Type: text/xml; charset=utf-8\r\n"
		     "Content-Disposition: attachment; filename=\"map.osm\"\r\n"
		     "Content-Encoding: %s\r\n"
		     "Cache-Control: private, max-age=0, must-revalidate\r\n"
		     "\r\n", encoding->name().c_str());
	
	// create the XML writer with the FCGI streams as output
	shared_ptr<xml_writer::output_buffer> out =
	  shared_ptr<fcgi_output_buffer>(new fcgi_output_buffer(request));
	out = encoding->output_buffer(out);
	xml_writer writer(out, true);
	
	try {
	  // call to write the map call
	  write_map(x, writer, bounds, changeset_cache);

	  // make sure all bytes have been written. note that the writer can
	  // throw an exception here, leaving the xml document in a 
	  // half-written state...
	  writer.flush();
	  out->flush();

	} catch (const xml_writer::write_error &e) {
	  // don't do anything - just go on to the next request.
	  logger::message(format("Caught write error, aborting request: %1%") % e.what());
	  
	} catch (const std::exception &e) {
	  // errors here are unrecoverable (fatal to the request but maybe
	  // not fatal to the process) since we already started writing to
	  // the client.
	  writer.start("error");
	  writer.text(e.what());
	  writer.end();
	}

        // log the completion time
	pt::ptime end_time(pt::second_clock::local_time());
	logger::message(format("Completed request in %1% returning %2% bytes") % (end_time - start_time) % out->written());

        // update the rate limiter
        limiter.update(ip, out->written());
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
      throw runtime_error("error accepting request.");
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
      int instances = options["instances"].as<int>();
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
	while (!terminate_requested && children.size() < instances) {
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
      // do work here
      process_requests(socket, options);
    }
  } catch (const pqxx::sql_error &er) {
    // Catch-all for any other postgres exceptions
    std::cerr << "Error: " << er.what() << std::endl
	      << "Caused by: " << er.query() << std::endl;
    return 1;

  } catch (const std::exception &e) {
    logger::message(e.what());
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;

  }

  return 0;
}
