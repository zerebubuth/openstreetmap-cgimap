#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <boost/format.hpp>

/**
 * Contains support for logging.
 */
namespace logger {

/**
 * Initialise logging.
 */
void initialise(const std::string &filename);

/**
 * Log a message.
 */
void message(const std::string &m);
void message(const boost::format &m);
}

#endif /* LOGGER_HPP */
