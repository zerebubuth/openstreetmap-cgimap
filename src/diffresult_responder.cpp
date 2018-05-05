#include "cgimap/config.hpp"
#include "cgimap/diffresult_responder.hpp"

using std::list;
using boost::shared_ptr;
namespace pt = boost::posix_time;

diffresult_responder::diffresult_responder(mime::type mt)
        : responder(mt) {}

diffresult_responder::~diffresult_responder() {}

void diffresult_responder::write(shared_ptr<output_formatter> formatter,
                                  const std::string &generator,
                                  const pt::ptime &now) {
  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  try {
    fmt.start_document(generator, "diffResult");

//    fmt.start_action(action_type::action_type_create);
//
//    fmt.end_action(action_type::action_type_create);

/*
 *
 * Example for diffResult output:
 *
 *      <node old_id="" new_id="" new_version=""/>
 *      <node old_id=""/>
 *      <way old_id="" new_id="" new_version=""/>
 *      <way old_id=""/>
 *      <relation old_id="" new_id="" new_version=""/>
 *      <relation old_id=""/>
 *
 */


//    // write all selected changesets
//    fmt.start_element_type(element_type_changeset);
//    sel->write_changesets(fmt, now);
//    fmt.end_element_type(element_type_changeset);

    fmt.end_document();



  } catch (const std::exception &e) {
    fmt.error(e);
  }

  fmt.end_document();
}

mime::type diffresult_responder::resource_type() {
  return (mime::text_xml);
}


