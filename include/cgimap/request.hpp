/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef REQUEST_HPP
#define REQUEST_HPP

#include "cgimap/http.hpp"

#include <chrono>
#include <string>
#include <string_view>


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
  request() = default;
  virtual ~request() = default;

  // get the value associated with a key in the request headers. returns NULL if
  // the key could not be found. this function can be called at any time.
  virtual const char *get_param(const char *key) const = 0;

  // get the current time of the request.
  virtual std::chrono::system_clock::time_point get_current_time() const = 0;

  // get payload provided for the request. this is useful in particular
  // for HTTP POST and PUT requests.
  virtual std::string get_payload() = 0;

  /********************** RESPONSE HEADER FUNCTIONS **************************/

  // set the status for the response. by default, the status is 500 and the user
  // will receive an error unless this is called with some other value. you can
  // call this function several times, but it is an error to call it after the
  // first call to any of the output functions.
  request& status(int code);

  // add a key-value header to the response. there may be some pre-existing
  // headers which are set by the output system, and some (e.g: CORS headers)
  // which are on by default for CGImap. it is an error to call this function
  // after a call to any of the output functions.
  request& add_header(const std::string &key, const std::string &value);

  // add a key-value header to the response like add_header, provided that
  // processing didn't trigger any error before calling any of the output
  // functions
  request& add_success_header(const std::string &key, const std::string &value);

  /********************** RESPONSE OUTPUT FUNCTIONS **************************/

  // return a handle to the output buffer to write body output. this function
  // should only be called after setting the status and any custom response
  // headers.
  output_buffer& get_buffer();

  // convenience functions to write body data. see `get_buffer()` for more
  // information on the constraints of calling this.
  int put(const char *, int);
  int put(std::string_view str);

  // call this to flush output to the client.
  void flush();

  /******************** RESPONSE FINISHING FUNCTIONS ************************/

  // call this when the entire response - including any body - has been
  // written. it is an error to call any output function after calling this.
  void finish();

  // dispose of any resources allocated to the request.
  virtual void dispose() = 0;

  /******************** RANDOM FUDGE FUNCTION *******************************/

  void set_default_methods(http::method);
  http::method methods() const;

protected:

  // this is called once, the first time an output function is called. the
  // implementing output system may use this to write out the complete set of
  // status & header information.
  virtual void write_header_info(int status, const http::headers_t &headers) = 0;

  // internal functions.
  // TODO: this is really bad design and indicates this should probably use
  // composition rather than inheritance.
  virtual output_buffer& get_buffer_internal() = 0;
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
  workflow_status m_workflow_status{status_NONE};

  // function to check and update the workflow
  void check_workflow(workflow_status this_stage);

  // the HTTP status code
  int m_status{500};

  // the headers to be written in the response
  http::headers_t m_headers;

  // the headers to be written in the response if process was successful
  http::headers_t m_success_headers;

  // allowed methods, to be returned to the client in the CORS headers.
  http::method m_methods{http::method::GET | http::method::HEAD | http::method::OPTIONS};
};

#endif /* REQUEST_HPP */
