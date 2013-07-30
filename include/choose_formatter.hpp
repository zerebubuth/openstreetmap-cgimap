#ifndef CHOOSE_FORMATTER_HPP
#define CHOOSE_FORMATTER_HPP

#include "handler.hpp"
#include "output_formatter.hpp"
#include "output_buffer.hpp"
#include "request.hpp"

#include <memory>
#include <boost/shared_ptr.hpp>

/**
 * chooses and initialises an output formatter which matches the requirements of
 * the Accept: headers in the request and the constraints of the responder 
 * resource that was selected by the path...
 */
boost::shared_ptr<output_formatter> choose_formatter(
	request &req, responder_ptr_t hptr, 
	boost::shared_ptr<output_buffer>);

#endif /* CHOOSE_FORMATTER_HPP */
