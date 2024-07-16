############
# unit tests
############
if(BUILD_TESTING)

    ##################
    # cgimap_staticxml
    ##################
    add_library(cgimap_staticxml)

    target_include_directories(cgimap_staticxml PUBLIC
        .)

    target_sources(cgimap_staticxml PRIVATE
        staticxml.cpp)

    target_link_libraries(cgimap_staticxml
        cgimap_common_compiler_options
        LibXml2::LibXml2
        cgimap_core)

    ###########
    # test_utils
    ###########
    add_library(test_utils)

    target_sources(test_utils PRIVATE
        test_database.cpp
        test_request.cpp)

    target_include_directories(test_utils PUBLIC
        .)

    target_link_libraries(test_utils PUBLIC
        cgimap_common_compiler_options
        cgimap_core
        cgimap_apidb
        Boost::program_options
        PQXX::PQXX)

    ###########
    # test_core
    ###########
    add_executable(test_core
        test_core.cpp
        test_request.cpp)

    target_link_libraries(test_core
        cgimap_common_compiler_options
        cgimap_core
        cgimap_staticxml
        Boost::program_options
        PQXX::PQXX)


    file(GLOB test_core_test_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*.testcore")
    foreach (test ${test_core_test_dirs})
    get_filename_component (TName "${test}" NAME_WE)
    add_test(NAME "${TName}.testcore" COMMAND test_core "${test}")
    endforeach ()


    ####################
    # test_parse_id_list
    ####################
    add_executable(test_parse_id_list
        test_parse_id_list.cpp
        test_request.cpp)

    target_link_libraries(test_parse_id_list
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_parse_id_list
        COMMAND test_parse_id_list)


    ###########
    # test_oauth2
    ###########
    add_executable(test_oauth2
        test_oauth2.cpp
        test_request.cpp)

    target_link_libraries(test_oauth2
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        PQXX::PQXX
        catch2)

    add_test(NAME test_oauth2
        COMMAND test_oauth2)


    ###########
    # test_http
    ###########
    add_executable(test_http
        test_http.cpp)

    target_link_libraries(test_http
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_http
        COMMAND test_http)


    #################
    # test_parse_time
    #################
    add_executable(test_parse_time
        test_parse_time.cpp)

    target_link_libraries(test_parse_time
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        PQXX::PQXX
        catch2)

    add_test(NAME test_parse_time
        COMMAND test_parse_time)


    ############################
    # test_parse_osmchange_input
    ############################
    add_executable(test_parse_osmchange_input
        test_parse_osmchange_input.cpp)

    target_link_libraries(test_parse_osmchange_input
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_parse_osmchange_input
        COMMAND test_parse_osmchange_input)


    ############################
    # test_parse_changeset_input
    ############################
    add_executable(test_parse_changeset_input
        test_parse_changeset_input.cpp)

    target_link_libraries(test_parse_changeset_input
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_parse_changeset_input
        COMMAND test_parse_changeset_input)


        ##########################
        # test_apidb_backend_nodes
        ##########################
        add_executable(test_apidb_backend_nodes
            test_apidb_backend_nodes.cpp
            test_formatter.cpp
            test_database.cpp
            test_request.cpp)

        target_link_libraries(test_apidb_backend_nodes
            cgimap_common_compiler_options
            cgimap_core
            cgimap_apidb
            Boost::program_options
            catch2)

        add_test(NAME test_apidb_backend_nodes
            COMMAND pg_virtualenv "$<TARGET_FILE:test_apidb_backend_nodes>" --db-schema "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")


        ###########################
        # test_apidb_backend_oauth2
        ###########################
        add_executable(test_apidb_backend_oauth2
            test_apidb_backend_oauth2.cpp
            test_formatter.cpp
            test_database.cpp
            test_request.cpp)

        target_link_libraries(test_apidb_backend_oauth2
            cgimap_common_compiler_options
            cgimap_core
            cgimap_apidb
            cgimap_staticxml
            Boost::program_options
            catch2)

        add_test(NAME test_apidb_backend_oauth2
            COMMAND pg_virtualenv "$<TARGET_FILE:test_apidb_backend_oauth2>" --db-schema "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")


        #############################
        # test_apidb_backend_historic
        #############################
        add_executable(test_apidb_backend_historic
            test_apidb_backend_historic.cpp
            test_formatter.cpp
            test_database.cpp
            test_request.cpp)

        target_link_libraries(test_apidb_backend_historic
            cgimap_common_compiler_options
            cgimap_core
            cgimap_apidb
            cgimap_staticxml
            Boost::program_options
            catch2)

        add_test(NAME test_apidb_backend_historic
            COMMAND pg_virtualenv "$<TARGET_FILE:test_apidb_backend_historic>" --db-schema "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")


        ###############################
        # test_apidb_backend_changesets
        ###############################
        add_executable(test_apidb_backend_changesets
            test_apidb_backend_changesets.cpp
            test_formatter.cpp
            test_database.cpp
            test_request.cpp)

        target_link_libraries(test_apidb_backend_changesets
            cgimap_common_compiler_options
            cgimap_core
            cgimap_apidb
            cgimap_staticxml
            Boost::program_options
            catch2)

        add_test(NAME test_apidb_backend_changesets
            COMMAND pg_virtualenv "$<TARGET_FILE:test_apidb_backend_changesets>" --db-schema "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")


        ########################################
        # test_apidb_backend_changeset_downloads
        ########################################
        add_executable(test_apidb_backend_changeset_downloads
            test_apidb_backend_changeset_downloads.cpp
            test_formatter.cpp
            test_database.cpp
            test_request.cpp)

        target_link_libraries(test_apidb_backend_changeset_downloads
            cgimap_common_compiler_options
            cgimap_core
            cgimap_apidb
            cgimap_staticxml
            Boost::program_options
            catch2)

        add_test(NAME test_apidb_backend_changeset_downloads
            COMMAND pg_virtualenv "$<TARGET_FILE:test_apidb_backend_changeset_downloads>" --db-schema "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")


        ######################################
        # test_apidb_backend_changeset_uploads
        ######################################
        add_executable(test_apidb_backend_changeset_uploads
            test_apidb_backend_changeset_uploads.cpp
            test_formatter.cpp
            test_database.cpp
            test_request.cpp)

        target_link_libraries(test_apidb_backend_changeset_uploads
            cgimap_common_compiler_options
            cgimap_core
            cgimap_apidb
            Boost::program_options
            Threads::Threads
            catch2)

        add_test(NAME test_apidb_backend_changeset_uploads
            COMMAND pg_virtualenv "$<TARGET_FILE:test_apidb_backend_changeset_uploads>" --db-schema "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")

    # define check alias target for autotools compatibility
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

    add_dependencies(check test_core
                           test_parse_id_list
                           test_oauth2
                           test_http
                           test_parse_time
                           test_parse_osmchange_input
                           test_parse_changeset_input)

      add_dependencies(check test_apidb_backend_nodes
                             test_apidb_backend_oauth2
                             test_apidb_backend_historic
                             test_apidb_backend_changesets
                             test_apidb_backend_changeset_downloads
                             test_apidb_backend_changeset_uploads)

endif()
