AM_LDFLAGS=@BOOST_LDFLAGS@
AM_CPPFLAGS=-I../include @FCGI_CPPFLAGS@ @LIBPQXX_CFLAGS@ @LIBXML_CFLAGS@ @LIBMEMCACHED_CFLAGS@ @BOOST_CPPFLAGS@

bin_PROGRAMS=../openstreetmap-cgimap
check_PROGRAMS=../test/test_core

lib_LTLIBRARIES=\
	libcgimap_core.la \
	libcgimap_fcgi.la
___openstreetmap_cgimap_LDADD=libcgimap_fcgi.la
___test_test_core_LDADD=

if ENABLE_APIDB
lib_LTLIBRARIES+=libcgimap_apidb.la
check_PROGRAMS+=\
	../test/test_apidb_backend_nodes \
	../test/test_apidb_backend_oauth \
	../test/test_apidb_backend_historic \
	../test/test_apidb_backend_changesets \
	../test/test_apidb_backend_changeset_downloads
___openstreetmap_cgimap_LDADD+=libcgimap_apidb.la
___test_test_core_LDADD+=libcgimap_apidb.la
___test_test_apidb_backend_nodes_LDADD=libcgimap_core.la libcgimap_apidb.la
___test_test_apidb_backend_oauth_LDADD=libcgimap_core.la libcgimap_apidb.la
___test_test_apidb_backend_historic_LDADD=libcgimap_core.la libcgimap_apidb.la
___test_test_apidb_backend_changesets_LDADD=libcgimap_core.la libcgimap_apidb.la
___test_test_apidb_backend_changeset_downloads_LDADD=libcgimap_core.la libcgimap_apidb.la
endif

if ENABLE_PGSNAPSHOT
lib_LTLIBRARIES+=libcgimap_pgsnapshot.la
___openstreetmap_cgimap_LDADD+=libcgimap_pgsnapshot.la
___test_test_core_LDADD+=libcgimap_pgsnapshot.la
if ENABLE_APIDB
___test_test_apidb_backend_nodes_LDADD+=libcgimap_pgsnapshot.la
___test_test_apidb_backend_oauth_LDADD+=libcgimap_pgsnapshot.la
___test_test_apidb_backend_historic_LDADD+=libcgimap_pgsnapshot.la
___test_test_apidb_backend_changesets_LDADD+=libcgimap_pgsnapshot.la
___test_test_apidb_backend_changeset_downloads_LDADD+=libcgimap_pgsnapshot.la
endif
endif

lib_LTLIBRARIES+=libcgimap_staticxml.la
___openstreetmap_cgimap_LDADD+=libcgimap_staticxml.la
___test_test_core_LDADD+=libcgimap_staticxml.la
if ENABLE_APIDB
___test_test_apidb_backend_nodes_LDADD+=libcgimap_staticxml.la
___test_test_apidb_backend_oauth_LDADD+=libcgimap_staticxml.la
___test_test_apidb_backend_historic_LDADD+=libcgimap_staticxml.la
___test_test_apidb_backend_changesets_LDADD+=libcgimap_staticxml.la
___test_test_apidb_backend_changeset_downloads_LDADD+=libcgimap_staticxml.la
endif

___openstreetmap_cgimap_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_core_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @CRYPTOPP_LIBS@
check_PROGRAMS+=../test/test_parse_id_list ../test/test_oauth ../test/test_http ../test/test_parse_time
___test_test_parse_id_list_LDADD=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_oauth_LDADD=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_http_LDADD=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_parse_time_LDADD=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_DATE_TIME_LIB@ @BOOST_SYSTEM_LIB@ @LIBPQXX_LIBS@

if ENABLE_APIDB
___test_test_apidb_backend_nodes_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_apidb_backend_oauth_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_apidb_backend_historic_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_apidb_backend_changesets_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
___test_test_apidb_backend_changeset_downloads_LDADD+=libcgimap_core.la @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@ @LIBPQXX_LIBS@ @CRYPTOPP_LIBS@
endif

################################################################################
___openstreetmap_cgimap_SOURCES=\
	main.cpp

___test_test_core_SOURCES=\
	../test/test_core.cpp \
	../test/test_request.cpp

___test_test_parse_id_list_SOURCES=\
	../test/test_parse_id_list.cpp

___test_test_oauth_SOURCES=\
	../test/test_oauth.cpp

___test_test_http_SOURCES=\
	../test/test_http.cpp

___test_test_parse_time_SOURCES=\
	../test/test_parse_time.cpp

if ENABLE_APIDB
___test_test_apidb_backend_nodes_SOURCES=\
	../test/test_apidb_backend_nodes.cpp \
	../test/test_formatter.cpp \
	../test/test_database.cpp \
	../test/test_request.cpp
___test_test_apidb_backend_oauth_SOURCES=\
	../test/test_apidb_backend_oauth.cpp \
	../test/test_formatter.cpp \
	../test/test_database.cpp \
	../test/test_request.cpp
___test_test_apidb_backend_historic_SOURCES=\
	../test/test_apidb_backend_historic.cpp \
	../test/test_formatter.cpp \
	../test/test_database.cpp \
	../test/test_request.cpp
___test_test_apidb_backend_changesets_SOURCES=\
	../test/test_apidb_backend_changesets.cpp \
	../test/test_formatter.cpp \
	../test/test_database.cpp \
	../test/test_request.cpp
___test_test_apidb_backend_changeset_downloads_SOURCES=\
	../test/test_apidb_backend_changeset_downloads.cpp \
	../test/test_formatter.cpp \
	../test/test_database.cpp \
	../test/test_request.cpp
endif

libcgimap_fcgi_la_SOURCES=\
	fcgi_request.cpp
libcgimap_fcgi_la_LIBADD=@FCGI_LIBS@

libcgimap_core_la_SOURCES=\
	backend.cpp \
	bbox.cpp \
	choose_formatter.cpp \
	data_selection.cpp \
	handler.cpp \
	http.cpp \
	logger.cpp \
	mime_types.cpp \
	oauth.cpp \
	osm_responder.cpp \
	osm_current_responder.cpp \
	osmchange_responder.cpp \
	output_formatter.cpp \
	output_writer.cpp \
	process_request.cpp \
	rate_limiter.cpp \
	request.cpp \
	request_helpers.cpp \
	router.cpp \
	routes.cpp \
	time.cpp \
	xml_formatter.cpp \
	xml_writer.cpp \
	zlib.cpp
libcgimap_core_la_LIBADD=@LIBXML_LIBS@ @YAJL_LIBS@ @LIBMEMCACHED_LIBS@ @BOOST_REGEX_LIB@ @BOOST_DATE_TIME_LIB@ @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@ @BOOST_LOCALE_LIB@  @CRYPTOPP_LIBS@

libcgimap_core_la_SOURCES+=\
	api06/map_handler.cpp \
	api06/handler_utils.cpp \
	api06/node_handler.cpp \
	api06/node_version_handler.cpp \
	api06/node_history_handler.cpp \
	api06/nodes_handler.cpp \
	api06/relation_handler.cpp \
	api06/relation_version_handler.cpp \
	api06/relations_handler.cpp \
	api06/relation_history_handler.cpp \
	api06/way_handler.cpp \
	api06/way_version_handler.cpp \
	api06/ways_handler.cpp \
	api06/way_history_handler.cpp \
	api06/relation_full_handler.cpp \
	api06/way_full_handler.cpp \
	api06/id_version_io.cpp \
	api06/changeset_handler.cpp \
	api06/changeset_download_handler.cpp

if ENABLE_EXPERIMENTAL
libcgimap_core_la_SOURCES+=\
	api06/node_ways_handler.cpp
endif

if ENABLE_API07
libcgimap_core_la_SOURCES+=api07/map_handler.cpp
endif

if HAVE_YAJL
libcgimap_core_la_SOURCES+=json_formatter.cpp json_writer.cpp
libcgimap_core_la_LIBADD+=@YAJL_LIBS@
endif

if ENABLE_APIDB
libcgimap_apidb_la_SOURCES=\
	backend/apidb/apidb.cpp \
	backend/apidb/writeable_pgsql_selection.cpp \
	backend/apidb/readonly_pgsql_selection.cpp \
	backend/apidb/common_pgsql_selection.cpp \
	backend/apidb/changeset.cpp \
	backend/apidb/quad_tile.cpp \
	backend/apidb/oauth_store.cpp \
	backend/apidb/utils.cpp
libcgimap_apidb_la_LIBADD=libcgimap_core.la @BOOST_DATE_TIME_LIB@ @LIBPQXX_LIBS@
endif

if ENABLE_PGSNAPSHOT
libcgimap_pgsnapshot_la_SOURCES=\
	backend/pgsnapshot/pgsnapshot.cpp \
	backend/pgsnapshot/snapshot_selection.cpp
libcgimap_pgsnapshot_la_LIBADD=libcgimap_core.la @LIBPQXX_LIBS@
endif

libcgimap_staticxml_la_SOURCES=\
	backend/staticxml/staticxml.cpp
libcgimap_staticxml_la_LIBADD=libcgimap_core.la @LIBXML_LIBS@
