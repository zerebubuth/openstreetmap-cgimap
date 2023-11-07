#ifndef OSMOBJECT_INPUT_FORMAT_HPP
#define OSMOBJECT_INPUT_FORMAT_HPP

#include "cgimap/api06/changeset_upload/node.hpp"

#include "parsers/saxparser.hpp"

namespace api06 {

class OSMObjectXMLParser : protected xmlpp::SaxParser {

protected:

  void on_enhance_exception(xmlParserInputPtr& location) override {

    try {
        throw;
    } catch (const xml_error& e) {
      throw_with_context(e, location);
    }
  }

  template <typename T>
  static void check_attributes(const char **attrs, T check) {

    if (attrs == NULL)
      return;

    while (*attrs) {
      check(attrs[0], attrs[1]);
      attrs += 2;
    }
  }

  void init_object(OSMObject &object, const char **attrs) {

    check_attributes(attrs, [&object](const char *name, const char *value) {
      if (!std::strcmp(name, "id")) {
        object.set_id(value);
      } else if (!std::strcmp(name, "changeset")) {
        object.set_changeset(value);
      } else if (!std::strcmp(name, "version")) {
        object.set_version(value);
      }
    });
  }

  void init_node(Node &node, const char **attrs) {

    check_attributes(attrs, [&node](const char *name, const char *value) {
      if (!std::strcmp(name, "lon")) {
        node.set_lon(value);
      } else if (!std::strcmp(name, "lat")) {
        node.set_lat(value);
      }
    });
  }

  void add_tag(OSMObject &o, const char **attrs) {

    std::optional<std::string> k;
    std::optional<std::string> v;

    check_attributes(attrs, [&k, &v](const char *name, const char *value) {

      if (name[0] == 'k' && name[1] == 0) {
        k = value;
      } else if (name[0] == 'v' && name[1] == 0) {
        v = value;
      }
    });

    if (!k)
      throw xml_error{
        fmt::format("Mandatory field k missing in tag element for {}",
         o.to_string())
      };

    if (!v)
      throw xml_error{
        fmt::format("Mandatory field v missing in tag element for {}",
         o.to_string())
      };

    o.add_tag(*k, *v);
  }

  // Include XML message location information where error occurred in exception
  template <typename TEx>
  void throw_with_context(TEx& e, xmlParserInputPtr& location) {

    // Location unknown
    if (location == nullptr)
      throw e;

    throw TEx{ fmt::format("{} at line {:d}, column {:d}",
               e.what(),
               location->line,
               location->col ) };
  }
};

} // namespace api06

#endif // OSMOBJECT_INPUT_FORMAT_HPP
