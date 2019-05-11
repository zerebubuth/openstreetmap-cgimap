
#include <memory>
#include <stdexcept>
#include <iostream>
#include <utility>
#include "cgimap/text_writer.hpp"


text_writer::text_writer(const std::string &file_name, bool indent) {
}

// create a new text writer using writer callback functions
text_writer::text_writer(std::shared_ptr<output_buffer> &out, bool indent) : out(out) {

}


text_writer::~text_writer() noexcept {

}

void text_writer::start(const std::string &name) {

}


void text_writer::text(const std::string &t) {
  out->write(t.c_str(), t.size());
}

void text_writer::end() {

}

void text_writer::flush() {

}

void text_writer::error(const std::string &s) {
  text(s);
}

