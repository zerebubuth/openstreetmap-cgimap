#include "cgimap/data_selection.hpp"

data_selection::~data_selection() {}

bool data_selection::supports_historical_versions() {
  return false;
}

int data_selection::select_historical_nodes(const std::vector<osm_edition_t> &) {
  throw std::runtime_error("data_selection does not support historical nodes");
}

int data_selection::select_historical_ways(const std::vector<osm_edition_t> &) {
  throw std::runtime_error("data_selection does not support historical ways");
}

int data_selection::select_historical_relations(const std::vector<osm_edition_t> &) {
  throw std::runtime_error("data_selection does not support historical relations");
}

data_selection::factory::~factory() {}
