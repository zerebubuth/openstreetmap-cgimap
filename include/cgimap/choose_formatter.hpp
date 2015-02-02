#ifndef CHOOSE_FORMATTER_HPP
#define CHOOSE_FORMATTER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/output_buffer.hpp"
#include "cgimap/request.hpp"

#include <memory>
#include <boost/shared_ptr.hpp>

/**
 * chooses and returns the best available mime type for a given request and
 * responder given the constraints in the Accept header and so forth.
 */
mime::type choose_best_mime_type(request &req, responder_ptr_t hptr);

/**
 * chooses and initialises an output formatter which matches the requirements of
 * the Accept: headers in the request and the constraints of the responder
 * resource that was selected by the path...
 */
boost::shared_ptr<output_formatter>
choose_formatter(request &req, responder_ptr_t hptr,
                 boost::shared_ptr<output_buffer>);

#endif /* CHOOSE_FORMATTER_HPP */
