############
# unit tests
############
if(BUILD_TESTING)

    set(DB_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/structure.sql")

    # Function to add test with conditional virtualenv command
    function(add_test_with_virtualenv test_name)
        if(ENABLE_PGVIRTUALENV)
            set(TEST_COMMAND pg_virtualenv)
        endif()

        list(APPEND TEST_COMMAND $<TARGET_FILE:${test_name}> --db-schema ${DB_SCHEMA})
        add_test(NAME ${test_name} COMMAND ${TEST_COMMAND})
    endfunction()

    # Function to add test with conditional virtualenv command
    function(add_test_with_virtualenv_and_directory test_name target test_directory)
        if(ENABLE_PGVIRTUALENV)
            set(TEST_COMMAND pg_virtualenv)
        endif()

        list(APPEND TEST_COMMAND $<TARGET_FILE:${target}> --test-directory ${test_directory} --db-schema ${DB_SCHEMA})
        add_test(NAME ${test_name} COMMAND ${TEST_COMMAND})
    endfunction()

    ####################
    # test_core_check
    ####################
    add_executable(test_core_check
        test_core_check.cpp
        test_core_helper.cpp
        test_request.cpp)

    target_link_libraries(test_core_check
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_core_check
        COMMAND test_core_check)

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


    ###########
    # test_utils
    ###########
    add_executable(test_utils
        test_utils.cpp)

    target_link_libraries(test_utils
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_utils
        COMMAND test_utils)


    ####################
    # test_parse_options
    ####################
    add_executable(test_parse_options
        test_parse_options.cpp)

    target_link_libraries(test_parse_options
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        PQXX::PQXX
        catch2)

    add_test(NAME test_parse_options
        COMMAND test_parse_options)

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


    ################################
    # test_parse_osmchange_xml_input
    ################################
    add_executable(test_parse_osmchange_xml_input
        test_parse_osmchange_xml_input.cpp)

    target_link_libraries(test_parse_osmchange_xml_input
        cgimap_common_compiler_options
        cgimap_core
        Boost::program_options
        catch2)

    add_test(NAME test_parse_osmchange_xml_input
        COMMAND test_parse_osmchange_xml_input)


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

    add_test_with_virtualenv(test_apidb_backend_nodes)


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
        Boost::program_options
        catch2)

    add_test_with_virtualenv(test_apidb_backend_oauth2)

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
        Boost::program_options
        catch2)

    add_test_with_virtualenv(test_apidb_backend_historic)

    ##########################
    # test_apidb_backend_roles
    ##########################
    add_executable(test_apidb_backend_roles
        test_apidb_backend_roles.cpp
        test_formatter.cpp
        test_database.cpp
        test_request.cpp)

    target_link_libraries(test_apidb_backend_roles
        cgimap_common_compiler_options
        cgimap_core
        cgimap_apidb
        Boost::program_options
        catch2)

    add_test_with_virtualenv(test_apidb_backend_roles)


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
        Boost::program_options
        catch2)

    add_test_with_virtualenv(test_apidb_backend_changesets)


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
        Boost::program_options
        catch2)

    add_test_with_virtualenv(test_apidb_backend_changeset_downloads)


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

    add_test_with_virtualenv(test_apidb_backend_changeset_uploads)


    #########################
    # test_apidb_backend_core
    #########################
    add_executable(test_apidb_backend_core
        test_apidb_backend_core.cpp
        test_apidb_importer.cpp
        test_core_helper.cpp
        test_formatter.cpp
        test_database.cpp
        test_request.cpp
        xmlparser.cpp)

    target_link_libraries(test_apidb_backend_core
        cgimap_common_compiler_options
        cgimap_core
        cgimap_apidb
        catch2
        Boost::program_options
        PQXX::PQXX)

    file(GLOB test_apidb_backend_core_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*.testcore")
    foreach (test ${test_apidb_backend_core_dirs})
    get_filename_component (TName "${test}" NAME_WE)
    add_test_with_virtualenv_and_directory("${TName}.db.testcore" test_apidb_backend_core "${test}")
    endforeach ()


    # define check alias target for autotools compatibility
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

    add_dependencies(check test_parse_id_list
                           test_core_check
                           test_oauth2
                           test_http
                           test_parse_time
                           test_parse_options
                           test_parse_osmchange_xml_input
                           test_parse_changeset_input
                           test_apidb_backend_nodes
                           test_apidb_backend_oauth2
                           test_apidb_backend_historic
                           test_apidb_backend_changesets
                           test_apidb_backend_changeset_downloads
                           test_apidb_backend_changeset_uploads
                           test_apidb_backend_roles
                           test_apidb_backend_core)

endif()
