

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include <sstream>





/* OSMChange_Tracking
 *
 * - Keeps track of old_id->new_id mappings in case of new objects
 *   as well as version number changes in case of modify and delete operations
 * - Returns diffResult XML message as response message for an API 0.6 upload request
 */


OSMChange_Tracking::OSMChange_Tracking() {
}

std::string OSMChange_Tracking::get_xml_diff_result() {

//	// diffResult response according to http://wiki.openstreetmap.org/wiki/API_v0.6#Response_10
//
//	std::stringbuf buf;
//	std::ostream os(&buf);
//
//	// TODO: Use proper XML writer to generate XML document
//
//	os << R"(<?xml version="1.0" encoding="UTF-8"?>)" << std::endl;
//
//	os << R"(<diffResult version="0.6" generator="OpenStreetMap server" copyright="OpenStreetMap and contributors" attribution="http://www.openstreetmap.org/copyright" license="http://opendatacommons.org/licenses/odbl/1-0/">)"
//			<< std::endl;
//
//	{
//		for (auto id : created_node_ids)
//			os << "  <node old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : modified_node_ids)
//			os << "  <node old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : already_deleted_node_ids)
//			os << "  <node old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : deleted_node_ids)
//			os << "  <node old_id=\"" << id << "\"/>" << std::endl;
//
//	}
//
//	{
//		for (auto id : created_way_ids)
//			os << "  <way old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : modified_way_ids)
//			os << "  <way old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : already_deleted_way_ids)
//			os << "  <way old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : deleted_way_ids)
//			os << "  <way old_id=\"" << id << "\"/>" << std::endl;
//
//	}
//
//	{
//		for (auto id : created_relation_ids)
//			os << "  <relation old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : modified_relation_ids)
//			os << "  <relation old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : already_deleted_relation_ids)
//			os << "  <relation old_id=\"" << id.old_id << "\" new_id=\""
//					<< id.new_id << "\" new_version=\"" << id.new_version
//					<< "\"/>" << std::endl;
//
//		for (auto id : deleted_relation_ids)
//			os << "  <relation old_id=\"" << id << "\"/>" << std::endl;
//
//	}
//
//	os << "</diffResult>" << std::endl;
//	return buf.str();
}

