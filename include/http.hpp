#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <map>
#include <stdexcept>

/**
 * Contains the generic HTTP methods and classes involved in the
 * application. CGI-specific stuff is elsewhere, but this stuff
 * should be theoretically re-usable in a non CGI context.
 */
namespace http {

  /**
   * Base class for HTTP protocol related exceptions. 
   *
   * Not directly constructable - use the derived classes instead.
   */
  class exception 
    : public std::exception {
  private:
    /// numerical status code, for more information see 
    /// http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
    const unsigned int code_;

    /// the header is a short description of the code, mainly for
    /// human consumption.
    const std::string header_;

    /// specific error message, meant entirely for humans to read.
    const std::string message_;
    
  protected:
    exception(unsigned int c, const std::string &h, const std::string &m); 
    
  public:
    virtual ~exception() throw();

    unsigned int code() const;
    const std::string &header() const;
    const char *what() const throw();
  };

  /**
   * An error which has caused the current request to fail which is
   * due to an internal error or code bug. The client might try again,
   * but there is no guarantee it will work. Use only for conditions
   * which are unrecoverable.
   */
  class server_error
    : public exception {
  public:
    server_error(const std::string &message);
  };

  /**
   * The client's request is badly-formed and cannot be serviced. Used
   * mainly for parse errors, or invalid data.
   */
  class bad_request
    : public exception {
  public:
    bad_request(const std::string &message);
  };

  /**
   * The client has attempted to use an HTTP method which is not 
   * supported on the receiving URI. This is a very specific error
   * and shouldn't be used except in this context.
   */
  class method_not_allowed
    : public exception {
  public:
    method_not_allowed(const std::string &message);
  };

  /**
   * Parses a query string into a key-value map.
   *
   * Expects an URL-encoded query string and returns a map with strings
   * in the key and value parts URL-encoded as well.
   */
  std::map<std::string, std::string>
  parse_params(const std::string &p);

}

#endif /* HTTP_HPP */
