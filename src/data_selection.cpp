#include "cgimap/data_selection.hpp"

data_selection::~data_selection() {}

#ifdef ENABLE_EXPERIMENTAL
void data_selection::write_changesets(output_formatter &, const boost::posix_time::ptime &) {
}

bool data_selection::supports_changesets() {
  return false;
}

int data_selection::select_changesets(const std::vector<osm_changeset_id_t> &) {
  return 0;
}

void data_selection::select_changeset_discussions() {
}
#endif /* ENABLE_EXPERIMENTAL */

data_selection::factory::~factory() {}
