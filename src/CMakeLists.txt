#############
# cgimap_core
#############
add_library(cgimap_core)

target_include_directories(cgimap_core PUBLIC
    ../include)

target_sources(cgimap_core PRIVATE
    backend.cpp
    backend.cpp
    bbox.cpp
    brotli.cpp
    choose_formatter.cpp
    handler.cpp
    http.cpp
    logger.cpp
    mime_types.cpp
    oauth2.cpp
    options.cpp
    osm_responder.cpp
    osm_changeset_responder.cpp
    osm_current_responder.cpp
    osm_diffresult_responder.cpp
    osmchange_responder.cpp
    output_formatter.cpp
    process_request.cpp
    rate_limiter.cpp
    request.cpp
    request_helpers.cpp
    router.cpp
    routes.cpp
    text_formatter.cpp
    text_responder.cpp
    text_writer.cpp
    time.cpp
    xml_formatter.cpp
    xml_writer.cpp
    zlib.cpp

    api06/changeset_close_handler.cpp
    api06/changeset_create_handler.cpp
    api06/changeset_download_handler.cpp
    api06/changeset_handler.cpp
    api06/changeset_update_handler.cpp
    api06/changeset_upload_handler.cpp
    api06/handler_utils.cpp
    api06/map_handler.cpp
    api06/node_handler.cpp
    api06/node_history_handler.cpp
    api06/node_relations_handler.cpp
    api06/nodes_handler.cpp
    api06/node_version_handler.cpp
    api06/node_ways_handler.cpp
    api06/relation_full_handler.cpp
    api06/relation_handler.cpp
    api06/relation_history_handler.cpp
    api06/relation_relations_handler.cpp
    api06/relations_handler.cpp
    api06/relation_version_handler.cpp
    api06/way_full_handler.cpp
    api06/way_handler.cpp
    api06/way_history_handler.cpp
    api06/way_relations_handler.cpp
    api06/ways_handler.cpp
    api06/way_version_handler.cpp
    api06/changeset_upload/osmchange_handler.cpp
    api06/changeset_upload/osmchange_tracking.cpp
    json_formatter.cpp
    json_writer.cpp
)

target_link_libraries(cgimap_core
    cgimap_common_compiler_options
    cgimap_libxml++
    ZLIB::ZLIB
    $<$<BOOL:${ENABLE_CRYPTOPP}>:CryptoPP::CryptoPP>
    Libmemcached::Libmemcached
    sjparser
    $<$<BOOL:${ENABLE_BROTLI}>:Brotli::common>
    $<$<BOOL:${ENABLE_BROTLI}>:Brotli::encoder>
    $<$<BOOL:${ENABLE_BROTLI}>:Brotli::decoder>
    YAJL::YAJL
    PQXX::PQXX)

#############
# cgimap_fcgi
#############
add_library(cgimap_fcgi)

target_include_directories(cgimap_fcgi PUBLIC
    ../include)

target_sources(cgimap_fcgi PRIVATE
    fcgi_request.cpp)

target_link_libraries(cgimap_fcgi
    cgimap_common_compiler_options
    Fcgi::Fcgi)



add_subdirectory(backend/apidb)

