#include "request.hpp"

request::~request() {
}

int request::put(const char *ptr, int len) {
   return get_buffer()->write(ptr, len);
}

int request::put(const std::string &str) {
   return get_buffer()->write(str.c_str(), str.size());
}

void request::flush() {
   get_buffer()->flush();
}
