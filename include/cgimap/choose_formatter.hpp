/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef CHOOSE_FORMATTER_HPP
#define CHOOSE_FORMATTER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/output_buffer.hpp"
#include "cgimap/request.hpp"

#include <memory>

/**
 * chooses and returns the best available mime type for a given request and
 * responder given the constraints in the Accept header and so forth.
 */
mime::type choose_best_mime_type(const request &req, const responder& hptr);

/**
 * creates and initialises an output formatter which matches the MIME type
 * passed in as an argument.
 */
std::unique_ptr<output_formatter>
create_formatter(mime::type best_type, output_buffer&);

#endif /* CHOOSE_FORMATTER_HPP */
