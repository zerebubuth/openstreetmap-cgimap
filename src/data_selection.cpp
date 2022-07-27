#include "cgimap/data_selection.hpp"


std::vector<std::string> psql_array_to_vector(std::string str) {
  std::vector<std::string> strs;
  std::ostringstream value;
  bool quotedValue = false;
  bool escaped = false;
  bool write = false;

  if (str == "{NULL}" || str == "")
    return strs;

  for (unsigned int i=1; i<str.size(); i++) {
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
