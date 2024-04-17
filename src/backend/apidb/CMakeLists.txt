##############
# cgimap_apidb
##############

    add_library(cgimap_apidb)

    target_include_directories(cgimap_apidb PUBLIC
        ../../../include)

    target_sources(cgimap_apidb PRIVATE
        apidb.cpp
        readonly_pgsql_selection.cpp
        common_pgsql_selection.cpp
        pgsql_update.cpp
        changeset.cpp
        quad_tile.cpp
        transaction_manager.cpp
        utils.cpp
        changeset_upload/changeset_updater.cpp
        changeset_upload/node_updater.cpp
        changeset_upload/relation_updater.cpp
        changeset_upload/way_updater.cpp
    )

    target_link_libraries(cgimap_apidb
        cgimap_common_compiler_options
        cgimap_core
        PQXX::PQXX)

