LDADD=@LIBPQXX_LIBS@ @FCGI_LIBS@ @YAJL_LIBS@ @LIBXML_LIBS@ @LIBMEMCACHED_LIBS@ @BOOST_REGEX_LIB@ @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_DATE_TIME_LIB@ @BOOST_SYSTEM_LIB@
AM_LDFLAGS=@BOOST_LDFLAGS@
AM_CPPFLAGS=-I../include @FCGI_CFLAGS@ @LIBPQXX_CFLAGS@ @LIBXML_CFLAGS@ @LIBMEMCACHED_CFLAGS@ @BOOST_CPPFLAGS@

bin_PROGRAMS=../map
################################################################################
___map_SOURCES=\
	backend.cpp \
	bbox.cpp \
	choose_formatter.cpp \
	data_selection.cpp \
	fcgi_helpers.cpp \
	handler.cpp \
	http.cpp \
	logger.cpp \
	main.cpp \
	mime_types.cpp \
	osm_responder.cpp \
	output_formatter.cpp \
	output_writer.cpp \
	rate_limiter.cpp \
	router.cpp \
	routes.cpp \
	xml_formatter.cpp \
	xml_writer.cpp \
	zlib.cpp

if ENABLE_APIDB
___map_SOURCES+=\
	backend/apidb/apidb.cpp \
	backend/apidb/writeable_pgsql_selection.cpp \
	backend/apidb/readonly_pgsql_selection.cpp \
	backend/apidb/changeset.cpp \
	backend/apidb/quad_tile.cpp
endif

if ENABLE_PGSNAPSHOT
___map_SOURCES+=\
	backend/pgsnapshot/pgsnapshot.cpp \
	backend/pgsnapshot/snapshot_selection.cpp
endif

___map_SOURCES+=\
	api06/map_handler.cpp \
	api06/node_handler.cpp \
	api06/nodes_handler.cpp \
	api06/relation_handler.cpp \
	api06/relations_handler.cpp \
	api06/way_handler.cpp \
	api06/ways_handler.cpp \
	api06/relation_full_handler.cpp \
	api06/way_full_handler.cpp

if ENABLE_API07
___map_SOURCES+=api07/map_handler.cpp
endif

if HAVE_YAJL
___map_SOURCES+=json_formatter.cpp json_writer.cpp
endif
