/* parse_error.h
 *
 * Copyright (C) 2002 The libxml++ development team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LIBXMLPP_PARSE_ERROR_H
#define __LIBXMLPP_PARSE_ERROR_H

#include "exception.hpp"

namespace xmlpp
{

/** This exception will be thrown when the parser encounters an error in the XML document.
 */
class parse_error : public exception
{
public:
  explicit parse_error(const std::string& message);
  ~parse_error() noexcept override;

  void raise() const override;
  exception* clone() const override;
};

} // namespace xmlpp

#endif // __LIBXMLPP_PARSE_ERROR_H
