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
 * creates and initialises an output formatter which matches the MIME type
 * passed in as an argument.
 */
boost::shared_ptr<output_formatter>
create_formatter(request &req, mime::type best_type,
                 boost::shared_ptr<output_buffer>);

#endif /* CHOOSE_FORMATTER_HPP */
