#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fmt/core.h>

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
}

#endif /* LOGGER_HPP */
