#ifndef TYPES_HPP
#define TYPES_HPP

#include <stdint.h>
#include <cassert>
#include <utility>

typedef uint64_t osm_user_id_t;
typedef int64_t osm_changeset_id_t;
typedef uint64_t osm_nwr_id_t;
typedef uint32_t tile_id_t;
typedef uint32_t osm_version_t;
typedef uint64_t osm_redaction_id_t;

// a edition of an element is the pair of its ID and version.
typedef std::pair<osm_nwr_id_t, osm_version_t> osm_edition_t;

#endif /* TYPES_HPP */
