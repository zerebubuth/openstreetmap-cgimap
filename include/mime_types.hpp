#ifndef MIME_TYPES_HPP
#define MIME_TYPES_HPP

#include <string>

/**
 * simple set of supported mime types.
 */
namespace mime {
enum type {
	unspecified_type, // a "null" type, used to indicate no choice.
	text_xml,
#ifdef HAVE_YAJL
	text_json,
#endif
	any_type // the "*/*" type used to mean that anything is acceptable.
};

std::string to_string(type);

}

#endif /* MIME_TYPES_HPP */
