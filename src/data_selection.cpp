#include "cgimap/data_selection.hpp"

data_selection::~data_selection() {}

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

data_selection::factory::~factory() {}


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
