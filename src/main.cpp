#include <mysql++/mysql++.h>
#include <mysql++/options.h>
#include <iostream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <fcgiapp.h>

#include "bbox.hpp"
#include "temp_tables.hpp"
#include "writer.hpp"
#include "split_tags.hpp"
#include "map.hpp"
#include "http.hpp"

using std::runtime_error;
using std::vector;
using std::string;
using std::map;
using std::ostringstream;

#define MAX_AREA 0.25

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
    http::parse_params(fcgi_get_env(request, "QUERY_STRING"));
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
       << "Content-type: text/html\r\n"
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

void
connect_db(mysqlpp::Connection &con,
	   mysqlpp::Connection &con2) {
  // get the DB parameters from the environment
  string db_name, db_host, db_user, db_pass, db_charset;
  if (!get_env("DB_NAME", db_name)) { 
    throw runtime_error("$DB_NAME not set."); 
  }
  if (!get_env("DB_HOST", db_host)) { 
    throw runtime_error("$DB_HOST not set."); 
  }
  if (!get_env("DB_USER", db_user)) { 
    throw runtime_error("$DB_USER not set."); 
  }
  if (!get_env("DB_PASS", db_pass)) { 
    throw runtime_error("$DB_PASS not set."); 
  }
  if (!get_env("DB_CHARSET", db_charset)) {
    db_charset = "utf8";
  }
  
  // set the connections to use the appropriate charset
  con.set_option(new mysqlpp::SetCharsetNameOption(db_charset));
  con2.set_option(new mysqlpp::SetCharsetNameOption(db_charset));
  
  // connect to the database.
  con.connect(db_user.c_str(), db_host.c_str(), 
	      db_user.c_str(), db_pass.c_str());
  con2.connect(db_user.c_str(), db_host.c_str(), 
	       db_user.c_str(), db_pass.c_str());
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

    // open the connections to mysql
    mysqlpp::Connection con, con2;
  
    // get the parameters for the connection from the environment
    // and connect to the database, throws exceptions if it fails.
    connect_db(con, con2);

    // create the request object for fcgi calls
    FCGX_Request request;
    if (FCGX_InitRequest(&request, 0, 0) != 0) {
      throw runtime_error("Couldn't initialise FCGX request structure.");
    }
    
    // enter the main loop
    while(FCGX_Accept_r(&request) >= 0) {
      try {
	// read all the input data..?
	
	// validate the input
	bbox bounds = validate_request(request);
	
	// create temporary tables of nodes, ways and relations which
	// are in or used by elements in the bbox
	tmp_nodes tn(con, bounds);
	tmp_ways tw(con);

	// write the response header
	FCGX_PutS("Status: 200 OK\r\n"
		  "Content-type: text/xml\r\n"
		  "\r\n", request.out);
	
	// create the XML writer with the FCGI streams as output
	fcgi_output_buffer out(request);
	xml_writer writer(out, true);
	
	try {
	  // call to write the map call
	  write_map(con, con2, writer, bounds);

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

  } catch (const mysqlpp::Exception& er) {
    // Catch-all for any other MySQL++ exceptions
    std::cerr << "Error: " << er.what() << std::endl;
    return 1;

  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;

  }

  return 0;
}
