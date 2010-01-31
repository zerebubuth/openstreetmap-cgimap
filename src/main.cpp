#include <pqxx/pqxx>
#include <iostream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <fcgiapp.h>
#include <memory>
#include <algorithm>

#include "bbox.hpp"
#include "temp_tables.hpp"
#include "writer.hpp"
#include "split_tags.hpp"
#include "map.hpp"
#include "http.hpp"
#include "logger.hpp"

using std::runtime_error;
using std::vector;
using std::string;
using std::map;
using std::ostringstream;
using std::auto_ptr;
using boost::shared_ptr;

namespace pt = boost::posix_time;

#define MAX_AREA 0.25
#define CACHE_SIZE 1000

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

  const map<string, string> params = 
    http::parse_params(get_query_string(request));
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
connect_db() {
  // get the DB parameters from the environment
  string db_name, db_host, db_user, db_pass, db_charset;
  if (!get_env("DB_NAME", db_name)) { 
    throw runtime_error("$DB_NAME not set."); 
  }
  if (!get_env("DB_HOST", db_host)) { 
    throw runtime_error("$DB_HOST not set."); 
  }
  if (!get_env("DB_CHARSET", db_charset)) {
    db_charset = "utf8";
  }
  
  ostringstream ostr;
  ostr << "dbname=" << db_name;
  ostr << " host=" << db_host;
  if (get_env("DB_USER", db_user)) {
    ostr << " user=" << db_user;
  }
  if (get_env("DB_PASS", db_pass)) {
    ostr << " password=" << db_pass;
  }

  // connect to the database.
  auto_ptr<pqxx::connection> con(new pqxx::connection(ostr.str()));

  // set the connections to use the appropriate charset
  con->set_client_encoding(db_charset);

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
    return FCGX_PutStr(buffer, len, r.out);
  }

  virtual int close() {
    // we don't actually close the FCGI output, as that happens
    // automatically on the next call to accept.
    return 0;
  }

  virtual ~fcgi_output_buffer() {
  }

  fcgi_output_buffer(FCGX_Request &req) 
    : r(req) {
  }

private:
  FCGX_Request &r;
};

int
main() {
  try {
    // initialise FCGI
    if (FCGX_Init() != 0) {
      throw runtime_error("Couldn't initialise FCGX library.");
    }

    // get the parameters for the connection from the environment
    // and connect to the database, throws exceptions if it fails.
    auto_ptr<pqxx::connection> con = connect_db();
    auto_ptr<pqxx::connection> cache_con = connect_db();

    // create the request object for fcgi calls
    FCGX_Request request;
    if (FCGX_InitRequest(&request, 0, 0) != 0) {
      throw runtime_error("Couldn't initialise FCGX request structure.");
    }

    // start a transaction using a second connection just for looking up 
    // users/changesets for the cache.
    pqxx::nontransaction cache_x(*cache_con, "changeset_cache");
    cache<long int, changeset> changeset_cache(boost::bind(fetch_changeset, boost::ref(cache_x), _1), CACHE_SIZE);

    logger() << "Initialised";

    // enter the main loop
    while(FCGX_Accept_r(&request) >= 0) {
      try {
	// read all the input data..?
	
	// validate the input
	bbox bounds = validate_request(request);

        pt::ptime start_time(pt::second_clock::local_time());
        logger() << "Started request for " << bounds.minlat << "," << bounds.minlon << "," << bounds.maxlat << "," << bounds.maxlon;

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

	} catch (const xml_writer::write_error &e) {
	  // don't do anything - just go on to the next request.
	  
	} catch (const std::exception &e) {
	  // errors here are unrecoverable (fatal to the request but maybe
	  // not fatal to the process) since we already started writing to
	  // the client.
	  writer.start("error");
	  writer.text(e.what());
	  writer.end();
	}

        pt::ptime end_time(pt::second_clock::local_time());
        logger() << "Completed request in " << (end_time - start_time);
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
    }

  } catch (const pqxx::sql_error &er) {
    // Catch-all for any other postgres exceptions
    std::cerr << "Error: " << er.what() << std::endl
	      << "Caused by: " << er.query() << std::endl;
    return 1;

  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;

  }

  return 0;
}
