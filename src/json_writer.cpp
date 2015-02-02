#include <yajl/yajl_gen.h>
#include <stdio.h>
#include <string.h>

#include "cgimap/json_writer.hpp"
#include "cgimap/config.hpp"

struct json_writer::pimpl_ {
  // not sure whether the config.hppas to live as long as the generator itself,
  // so seems best to be on the safe side.
  yajl_gen gen;

#ifdef HAVE_YAJL2
  yajl_alloc_funcs alloc_funcs;
#else
  yajl_gen_config config;
#endif
};

static void wrap_write(void *context, const char *str, unsigned int len) {
  output_buffer *out = static_cast<output_buffer *>(context);
  if (out == 0) {
    throw output_writer::write_error(
        "Output buffer was NULL in json_writer wrap_write().");
  }

  int wrote_len = out->write(str, len);

  if (wrote_len != int(len)) {
    throw output_writer::write_error(
        "Output buffer wrote a different amount than was expected.");
  }
}

json_writer::json_writer(boost::shared_ptr<output_buffer> &out, bool indent)
    : pimpl(new pimpl_()) {
#ifdef HAVE_YAJL2
  pimpl->gen = yajl_gen_alloc(NULL);

#else  /* older version of YAJL */
  // setup whether the generator should produce pretty output
  if (indent) {
    pimpl->config.beautify = 1;
    pimpl->config.indentString = " ";
  } else {
    pimpl->config.beautify = 0;
    pimpl->config.indentString = "";
  }

  pimpl->gen = yajl_gen_alloc2(&wrap_write, &pimpl->config, NULL, out.get());
#endif /* HAVE_YAJL2 */

  if (pimpl->gen == 0) {
    throw std::runtime_error("error creating json writer.");
  }

#ifdef HAVE_YAJL2
  if (indent) {
    yajl_gen_config(pimpl->gen, yajl_gen_beautify, 1);
    yajl_gen_config(pimpl->gen, yajl_gen_indent_string, " ");
  } else {
    yajl_gen_config(pimpl->gen, yajl_gen_beautify, 0);
    yajl_gen_config(pimpl->gen, yajl_gen_indent_string, "");
  }
  yajl_gen_config(pimpl->gen, yajl_gen_print_callback, &wrap_write,
                  (void *)out.get());
#endif /* HAVE_YAJL2 */
}

json_writer::~json_writer() throw() {
  yajl_gen_clear(pimpl->gen);
  yajl_gen_free(pimpl->gen);
  delete pimpl;
}

void json_writer::start_object() { yajl_gen_map_open(pimpl->gen); }

void json_writer::object_key(const std::string &s) {
  yajl_gen_string(pimpl->gen, (const unsigned char *)s.c_str(), s.size());
}

void json_writer::end_object() { yajl_gen_map_close(pimpl->gen); }

void json_writer::entry_bool(bool b) { yajl_gen_bool(pimpl->gen, b ? 1 : 0); }

void json_writer::entry_int(int i) { yajl_gen_integer(pimpl->gen, i); }

void json_writer::entry_int(unsigned long int i) {
  yajl_gen_integer(pimpl->gen, i);
}

void json_writer::entry_int(unsigned long long int i) {
  yajl_gen_integer(pimpl->gen, i);
}

void json_writer::entry_double(double d) {
  // this is the only way, it seems, to use a fixed format for double output.
  static char buffer[12];
  snprintf(buffer, 12, "%.7f", d);
  yajl_gen_number(pimpl->gen, buffer, strlen(buffer));
}

void json_writer::entry_string(const std::string &s) {
  yajl_gen_string(pimpl->gen, (const unsigned char *)s.c_str(), s.size());
}

void json_writer::start_array() { yajl_gen_array_open(pimpl->gen); }

void json_writer::end_array() { yajl_gen_array_close(pimpl->gen); }

void json_writer::flush() { yajl_gen_clear(pimpl->gen); }

void json_writer::error(const std::string &s) {
  start_object();
  object_key("error");
  entry_string(s);
  end_object();
}
