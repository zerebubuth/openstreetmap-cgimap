#include <yajl/yajl_gen.h>
#include <stdio.h>
#include <string.h>

#include "json_writer.hpp"
#include "writer.hpp"

struct json_writer::pimpl_ {
  // not sure whether the config has to live as long as the generator itself, so
  // seems best to be on the safe side.
  yajl_gen_config config;
  yajl_gen gen;
};

static void wrap_write(void *context, const char *str, unsigned int len) {
  output_buffer *out = static_cast<output_buffer *>(context);
  if (out == 0) {
    throw xml_writer::write_error("Output buffer was NULL in json_writer wrap_write().");
  }

  int wrote_len = out->write(str, len);

  if (wrote_len != len) {
    throw xml_writer::write_error("Output buffer wrote a different amount than was expected.");
  }
}

json_writer::json_writer(boost::shared_ptr<output_buffer> &out, bool indent)
  : pimpl(new pimpl_()) {
  // setup whether the generator should produce pretty output
  if (indent) {
    pimpl->config.beautify = 1;
    pimpl->config.indentString = " ";
  } else {
    pimpl->config.beautify = 0;
    pimpl->config.indentString = "";
  }

  pimpl->gen = yajl_gen_alloc2(&wrap_write, &pimpl->config, NULL, out.get());

  if (pimpl->gen == 0) {
    throw std::runtime_error("error creating json writer.");
  }
}

json_writer::~json_writer() {
  yajl_gen_clear(pimpl->gen);
  yajl_gen_free(pimpl->gen);
  delete pimpl;
}

void 
json_writer::start_object() {
  yajl_gen_map_open(pimpl->gen);
}

void 
json_writer::object_key(const std::string &s) {
  yajl_gen_string(pimpl->gen, (const unsigned char *)s.c_str(), s.size());
}

void 
json_writer::end_object() {
  yajl_gen_map_close(pimpl->gen);
}

void 
json_writer::entry_bool(bool b) {
  yajl_gen_bool(pimpl->gen, b ? 1 : 0);
}

void 
json_writer::entry_int(int i) {
  yajl_gen_integer(pimpl->gen, i);
}

void 
json_writer::entry_int(long int i) {
  yajl_gen_integer(pimpl->gen, i);
}

void 
json_writer::entry_int(long long int i) {
  yajl_gen_integer(pimpl->gen, i);
}

void 
json_writer::entry_double(double d) {
  // this is the only way, it seems, to use a fixed format for double output.
  static char buffer[12];
  snprintf(buffer, 12, "%.7f", d);
  yajl_gen_number(pimpl->gen, buffer, strlen(buffer));
}

void 
json_writer::entry_string(const std::string &s) {
  yajl_gen_string(pimpl->gen, (const unsigned char *)s.c_str(), s.size());
}

void 
json_writer::start_array() {
  yajl_gen_array_open(pimpl->gen);  
}

void 
json_writer::end_array() {
  yajl_gen_array_close(pimpl->gen);  
}
