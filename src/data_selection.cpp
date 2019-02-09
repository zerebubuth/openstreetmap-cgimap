#include "cgimap/data_selection.hpp"

data_selection::~data_selection() = default;

bool data_selection::supports_historical_versions() {
  return false;
}

int data_selection::select_historical_nodes(const std::vector<osm_edition_t> &) {
  throw std::runtime_error("data_selection does not support historical nodes");
}

int data_selection::select_nodes_with_history(const std::vector<osm_nwr_id_t> &) {
  throw std::runtime_error("data_selection does not support historical nodes");
}

int data_selection::select_historical_ways(const std::vector<osm_edition_t> &) {
  throw std::runtime_error("data_selection does not support historical ways");
}

int data_selection::select_ways_with_history(const std::vector<osm_nwr_id_t> &) {
  throw std::runtime_error("data_selection does not support historical ways");
}

int data_selection::select_historical_relations(const std::vector<osm_edition_t> &) {
  throw std::runtime_error("data_selection does not support historical relations");
}

int data_selection::select_relations_with_history(const std::vector<osm_nwr_id_t> &) {
  throw std::runtime_error("data_selection does not support historical relations");
}

void data_selection::set_redactions_visible(bool) {
  throw std::runtime_error("data_selection does not support redactions");
}

int data_selection::select_historical_by_changesets(
  const std::vector<osm_changeset_id_t> &) {
  throw std::runtime_error("data_selection does not support historical "
    "versions or changesets");
}

void data_selection::write_changesets(output_formatter &, const std::chrono::system_clock::time_point &) {
}

bool data_selection::supports_changesets() {
  return false;
}

int data_selection::select_changesets(const std::vector<osm_changeset_id_t> &) {
  return 0;
}

void data_selection::select_changeset_discussions() {
}

bool data_selection::supports_user_details() {
  return false;
}

bool data_selection::is_user_blocked(const osm_user_id_t) {

  return false;
}

bool data_selection::get_user_id_pass(const std::string&, osm_user_id_t &, std::string &, std::string &) {
  return false;
}


data_selection::factory::~factory() = default;


std::vector<std::string> psql_array_to_vector(std::string str) {
  std::vector<std::string> strs;
  std::ostringstream value;
  bool quotedValue = false, escaped = false, write = false;

  if (str == "{NULL}" || str == "")
    return strs;

  for (int i=1; i<str.size(); i++) {
    if (str[i]==',') {
      if (quotedValue) {
        value<<",";
      } else {
        write = true;
      }
    } else if (str[i]=='\"') {
      if (escaped) {
        value << "\"";
        escaped = false;
      } else if (quotedValue) {
        quotedValue=false;
      } else {
        quotedValue = true;
      }
    } else if (str[i]=='\\'){
      if (escaped) {
        value << "\\";
        escaped = false;
      } else {
        escaped = true;
      }
    } else if (str[i]=='}') {
      if(quotedValue){
        value << "}";
      } else {
        write = true;
      }
    } else {
      value<<str[i];
    }

    if (write) {
      strs.push_back(value.str());
      value.str("");
      value.clear();
      write=false;
    }
  }
  return strs;
}
