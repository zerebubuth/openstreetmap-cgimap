#ifndef API06_CHANGESET_UPLOAD_CHANGESET_UPDATER
#define API06_CHANGESET_UPLOAD_CHANGESET_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

namespace api06 {

class Changeset_Updater {

public:
  virtual ~Changeset_Updater() = default;

  virtual void lock_current_changeset() = 0;

  virtual void update_changeset(uint32_t num_new_changes, bbox_t bbox) = 0;
};

} // namespace api06

#endif
