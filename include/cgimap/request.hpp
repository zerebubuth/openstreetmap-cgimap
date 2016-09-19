#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

// forward declaration of output_buffer, which is only needed here by
// reference.
struct output_buffer;

/**
 * object representing the state of the client request & response.
 *
 * this object presents an interface for working with the client request after
 * it has been read and parsed from the client. this state includes writing the
 * response and any headers which need to go back to the client.
 *
 * the request's workflow is divided into sections, and certain functions can
 * only be called in certain sections. mainly, this is so that we can ensure
 * that writing the status and headers are guaranteed to come before writing the
 * body.
 */
struct request {
  request();
  virtual ~request();

  // get the value associated with a key in the request headers. returns NULL if
  // the key could not be found. this function can be called at any time.
  virtual const char *get_param(const char *key) = 0;

  // get the current time of the request.
  virtual boost::posix_time::ptime get_current_time() const = 0;

  /********************** RESPONSE HEADER FUNCTIONS **************************/

  // set the status for the response. by default, the status is 500 and the user
  // will receive an error unless this is called with some other value. you can
  // call this function several times, but it is an error to call it after the
  // first call to any of the output functions.
  void status(int code);

  // add a key-value header to the response. there may be some pre-existing
  // headers which are set by the output system, and some (e.g: CORS headers)
  // which are on by default for CGImap. it is an error to call this function
  // after a call to any of the output functions.
  void add_header(const std::string &key, const std::string &value);

  /********************** RESPONSE OUTPUT FUNCTIONS **************************/

  // return a handle to the output buffer to write body output. this function
  // should only be called after setting the status and any custom response
  // headers.
  boost::shared_ptr<output_buffer> get_buffer();

  // convenience functions to write body data. see `get_buffer()` for more
  // information on the constraints of calling this.
  int put(const char *, int);
  int put(const std::string &);

  // call this to flush output to the client.
  void flush();

  /******************** RESPONSE FINISHING FUNCTIONS ************************/

  // call this when the entire response - including any body - has been
  // written. it is an error to call any output function after calling this.
  void finish();

  // dispose of any resources allocated to the request.
  virtual void dispose() = 0;

protected:
  typedef std::vector<std::pair<std::string, std::string> > headers_t;

  // this is called once, the first time an output function is called. the
  // implementing output system may use this to write out the complete set of
  // status & header information.
  virtual void write_header_info(int status, const headers_t &headers) = 0;

  // internal functions.
  // TODO: this is really bad design and indicates this should probably use
  // composition rather than inheritance.
  virtual boost::shared_ptr<output_buffer> get_buffer_internal() = 0;
  virtual void finish_internal() = 0;

  // reset the state of the request back to blank for re-use.
  void reset();

private:
  // to track what stage of the workflow the response is currently in.
  enum workflow_status {
    status_NONE = 0,
    status_HEADERS = 1,
    status_BODY = 2,
    status_FINISHED = 3
  };
  workflow_status m_workflow_status;

  // function to check and update the workflow
  void check_workflow(workflow_status this_stage);

  // the HTTP status code
  int m_status;

  // the headers to be written in the response
  headers_t m_headers;
};

#endif /* REQUEST_HPP */
