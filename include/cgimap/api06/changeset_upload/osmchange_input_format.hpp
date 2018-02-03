#ifndef OSMCHANGE_INPUT_FORMAT_HPP
#define OSMCHANGE_INPUT_FORMAT_HPP

/*


NOTE: This is based on a heavily modified xml_input_format.hpp version

*/

#include "parser_callback.hpp"
#include "osmobject.hpp"
#include "node.hpp"
#include "way.hpp"
#include "relation.hpp"
#include "types.hpp"


#include <expat.h>
#include <expat_external.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

struct xml_error : public std::runtime_error {

        uint64_t line = 0;
        uint64_t column = 0;
        XML_Error error_code;
        std::string error_string;

        explicit xml_error(const XML_Parser& parser) :
                std::runtime_error(std::string{"XML parsing error at line "}
                                + std::to_string(XML_GetCurrentLineNumber(parser))
                                + ", column "
                                + std::to_string(XML_GetCurrentColumnNumber(parser))
                                + ": "
                                + XML_ErrorString(XML_GetErrorCode(parser))),
                line(XML_GetCurrentLineNumber(parser)),
                column(XML_GetCurrentColumnNumber(parser)),
                error_code(XML_GetErrorCode(parser)),
                error_string(XML_ErrorString(error_code)) {
        }

        explicit xml_error(const std::string& message) :
                std::runtime_error(message),
                error_code(),
                error_string(message) {
        }

};


class OSMChangeXMLParser {

        enum class context {
                root,
                top,
                in_create,
                in_modify,
                in_delete,
                node,
                way,
                relation,
                in_object
        };

        context m_context = context::root;
        context m_operation_context = context::root;
        context m_last_context = context::root;

        operation m_operation = operation::op_undefined;

        Parser_Callback * m_callback;

        std::unique_ptr< Node >     m_node{};
        std::unique_ptr< Way >      m_way{};
        std::unique_ptr< Relation > m_relation{};

        bool m_if_unused = false;


        template <typename T>
        class ExpatXMLParser {

                XML_Parser m_parser;

                static void XMLCALL start_element_wrapper(void* data, const XML_Char* element, const XML_Char** attrs) {
                        static_cast<OSMChangeXMLParser*>(data)->start_element(element, attrs);
                }

                static void XMLCALL end_element_wrapper(void* data, const XML_Char* element) {
                        static_cast<OSMChangeXMLParser*>(data)->end_element(element);
                }

                static void entity_declaration_handler(void*,
                                const XML_Char*,
                                int ,
                                const XML_Char*,
                                int,
                                const XML_Char*,
                                const XML_Char*,
                                const XML_Char*,
                                const XML_Char*) {
                        throw xml_error{"XML entities are not supported"};
                }

        public:

                explicit ExpatXMLParser(T* callback_object) :
                        m_parser(XML_ParserCreate(nullptr)) {
                        if (!m_parser) {
                                throw std::runtime_error{"Internal error: Can not create parser"};
                        }
                        XML_SetUserData(m_parser, callback_object);
                        XML_SetElementHandler(m_parser, start_element_wrapper, end_element_wrapper);
                        XML_SetEntityDeclHandler(m_parser, entity_declaration_handler);
                }

                ExpatXMLParser(const ExpatXMLParser&) = delete;
                ExpatXMLParser(ExpatXMLParser&&) = delete;

                ExpatXMLParser& operator=(const ExpatXMLParser&) = delete;
                ExpatXMLParser& operator=(ExpatXMLParser&&) = delete;

                ~ExpatXMLParser() noexcept {
                        XML_ParserFree(m_parser);
                }

                void operator()(const std::string& data, bool last) {
                        if (XML_Parse(m_parser, data.data(), static_cast<int>(data.size()), last) == XML_STATUS_ERROR) {
                                throw xml_error{m_parser};
                        }
                }

        }; // class ExpatXMLParser

        template <typename T>
        static void check_attributes(const XML_Char** attrs, T check) {
                while (*attrs) {
                        check(attrs[0], attrs[1]);
                        attrs += 2;
                }
        }

        void init_object(OSMObject& object, const XML_Char** attrs) {

                object.set_visible(m_operation != operation::op_delete);

                check_attributes(attrs, [&object](const XML_Char* name, const XML_Char* value) {
                        if (!std::strcmp(name, "id")) {
                        object.set_id(value);
                        } else if (!std::strcmp(name, "visible")) {
                        object.set_visible(value);
                        } else if (!std::strcmp(name, "changeset")) {
                        object.set_changeset(value);
                        } else if (!std::strcmp(name, "version")) {
                        object.set_version(value);
                        }  // don't parse any other attributes here
                });

                if (m_operation == operation::op_delete) {
                        if (object.visible()) {
                                throw xml_error{"Deleted object cannot be set to visible"};
                        }
                }

                if (m_operation == operation::op_create) {
                         // we always override version number for create operations (they are not mandatory)
                        object.set_version(0u);
                } else if (m_operation == operation::op_delete ||
                                   m_operation == operation::op_modify) {
                        // objects for other operations must have a positive version number
                        if (object.version() < 1){
                                throw xml_error{"Invalid version number "};
                        }
                }
        }

        void init_node(Node& node, const XML_Char** attrs)
        {
                check_attributes(attrs, [&node](const XML_Char* name, const XML_Char* value) {
                        if (!std::strcmp(name, "lon")) {
                                node.set_lon(value);
                        } else if (!std::strcmp(name, "lat")) {
                                node.set_lat(value);
                        }
                });
        }

        void get_tag(OSMObject & o, const XML_Char** attrs) {
                const char* k = "";
                const char* v = "";

                check_attributes(attrs, [&k, &v](const XML_Char* name, const XML_Char* value) {
                        if (name[0] == 'k' && name[1] == 0) {
                                k = value;
                        } else if (name[0] == 'v' && name[1] == 0) {
                                v = value;
                        }
                });

                o.add_tag(k, v);
        }

        void start_element(const XML_Char* element, const XML_Char** attrs) {

                switch (m_context) {
                        case context::root:
                                if (!std::strcmp(element, "osmChange")) {

                                } else {
                                        throw xml_error{std::string{"Unknown top-level element, expecting osmChange: "} + element};
                                }
                                m_context = context::top;
                                break;

                        case context::top:

                                if (!std::strcmp(element, "create")) {
                                        m_context = context::in_create;
                                        m_operation_context = context::in_create;
                                        m_operation = operation::op_create;
                                } else if (!std::strcmp(element, "modify")) {
                                        m_context = context::in_modify;
                                        m_operation_context = context::in_modify;
                                        m_operation = operation::op_modify;
                                } else if (!std::strcmp(element, "delete")) {
                                        m_if_unused = false;
                                        check_attributes(attrs, [&](const XML_Char* name, const XML_Char*) {
                                                if (!std::strcmp(name, "if-unused")) {
                                                        m_if_unused = true;
                                                }
                                        });
                                        m_context = context::in_delete;
                                        m_operation_context = context::in_delete;
                                        m_operation = operation::op_delete;
                                } else {
                                        throw xml_error{std::string{"Unknown operation: "} + element};
                                }
                                break;

                        case context::in_delete:
                        case context::in_create:
                        case context::in_modify:

                                if (!std::strcmp(element, "node")) {
                                        m_node.reset(new Node{});
                                        init_object(*m_node, attrs);
                                        init_node(*m_node, attrs);
                                        m_context = context::node;
                                } else if (!std::strcmp(element, "way")) {
                                        m_way.reset(new Way{});
                                        init_object(*m_way, attrs);
                                        m_context = context::way;
                                } else if (!std::strcmp(element, "relation")) {
                                        m_relation.reset(new Relation{});
                                        init_object(*m_relation, attrs);
                                        m_context = context::relation;
                                } else {
                                        throw new xml_error{std::string{"Unknown element, expecting node, way or relation: "} + element};
                                }
                                break;

                        case context::node:
                                m_last_context = context::node;
                                m_context = context::in_object;
                                if (!std::strcmp(element, "tag")) {
                                        get_tag(*m_node, attrs);
                                }
                                break;
                        case context::way:
                                m_last_context = context::way;
                                m_context = context::in_object;
                                if (!std::strcmp(element, "nd")) {
                                        check_attributes(attrs, [&](const XML_Char* name, const XML_Char* value) {
                                                if (!std::strcmp(name, "ref")) {
                                                        m_way->add_way_node(value);
                                                }
                                        });
                                } else if (!std::strcmp(element, "tag")) {
                                        get_tag(*m_way, attrs);
                                }
                                break;
                        case context::relation:
                                m_last_context = context::relation;
                                m_context = context::in_object;
                                if (!std::strcmp(element, "member")) {
                                        RelationMember member;
                                        check_attributes(attrs, [&member](const XML_Char* name, const XML_Char* value) {
                                                if (!std::strcmp(name, "type")) {
                                                        member.set_type(value);
                                                } else if (!std::strcmp(name, "ref")) {
                                                        member.set_ref(value);
                                                } else if (!std::strcmp(name, "role")) {
                                                        member.set_role(value);
                                                }
                                        });
                                        if (!member.is_valid()) {
                                                throw xml_error{"Missing ref on relation member"};
                                        }
                                        m_relation->add_member(member);
                                } else if (!std::strcmp(element, "tag")) {
                                        get_tag(*m_relation, attrs);
                                }
                                break;
                        case context::in_object:
                                throw xml_error{"xml file nested too deep"};
                                break;
                }
        }

        void end_element(const XML_Char* element) {

                switch (m_context) {
                        case context::root:
                                assert(false);
                                break;
                        case context::top:
                                assert(!std::strcmp(element, "osmChange"));
                                m_context = context::root;
                                m_operation = operation::op_undefined;
                                break;
                        case context::in_create:
                                assert(!std::strcmp(element, "create"));
                                m_context = context::top;
                                m_operation_context = context::top;
                                m_operation = operation::op_undefined;
                                break;
                        case context::in_modify:
                                assert(!std::strcmp(element, "modify"));
                                m_context = context::top;
                                m_operation_context = context::top;
                                m_operation = operation::op_undefined;
                                break;
                        case context::in_delete:
                                assert(!std::strcmp(element, "delete"));
                                m_context = context::top;
                                m_operation_context = context::top;
                                m_operation = operation::op_undefined;
                                m_if_unused = false;
                                break;
                        case context::node:
                                assert(!std::strcmp(element, "node"));
                                if (!m_node->is_valid(m_operation))
                                        throw std::runtime_error ("Node does not include all mandatory fields");
                                m_callback->node(*m_node, m_operation, m_if_unused);
                                m_node.reset(new Node{});
                                m_context = m_operation_context;
                                break;
                        case context::way:
                                assert(!std::strcmp(element, "way"));
                                if (!m_way->is_valid())
                                        throw std::runtime_error ("Way does not include all mandatory fields");
                                m_callback->way(*m_way, m_operation, m_if_unused);
                                m_way.reset(new Way{});
                                m_context = m_operation_context;
                                break;
                        case context::relation:
                                assert(!std::strcmp(element, "relation"));
                                if (!m_relation->is_valid())
                                        throw std::runtime_error ("Relation does not include all mandatory fields");
                                m_callback->relation(*m_relation, m_operation, m_if_unused);
                                m_relation.reset(new Relation{});
                                m_context = m_operation_context;
                                break;
                        case context::in_object:
                                m_context = m_last_context;
                                break;
                }
        }


public:

        explicit OSMChangeXMLParser(Parser_Callback* callback)  : m_callback(callback){

        }

        OSMChangeXMLParser(const OSMChangeXMLParser&) = delete;
        OSMChangeXMLParser& operator=(const OSMChangeXMLParser&) = delete;

        OSMChangeXMLParser(OSMChangeXMLParser&&) = delete;
        OSMChangeXMLParser& operator=(OSMChangeXMLParser&&) = delete;

        void process_message(std::string data) {

                ExpatXMLParser<OSMChangeXMLParser> parser{this};

                parser(data, true);

// TODO: streaming

//              while (!input_done()) {
//                      const std::string data{get_input()};
//                      parser(data, input_done());
//              }

        }
};



#endif // OSMCHANGE_INPUT_FORMAT_HPP

