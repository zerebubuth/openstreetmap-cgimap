LDADD=@LIBPQXX_LIBS@ @FCGI_LIBS@ @YAJL_LIBS@ @LIBXML_LIBS@ @LIBMEMCACHED_LIBS@ @BOOST_REGEX_LIB@ @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_DATE_TIME_LIB@
AM_LDFLAGS=@BOOST_LDFLAGS@
AM_CPPFLAGS=-I../include @FCGI_CFLAGS@ @LIBPQXX_CFLAGS@ @LIBXML_CFLAGS@ @LIBMEMCACHED_CFLAGS@ @BOOST_CPPFLAGS@

bin_PROGRAMS=../map
################################################################################
___map_SOURCES=\
	bbox.cpp \
	changeset.cpp \
	choose_formatter.cpp \
	data_selection.cpp \
	fcgi_helpers.cpp \
	handler.cpp \
	http.cpp \
	logger.cpp \
	main.cpp \
	map_handler.cpp \
	mime_types.cpp \
	node_handler.cpp \
	nodes_handler.cpp \
	osm_responder.cpp \
	output_formatter.cpp \
	output_writer.cpp \
	quad_tile.cpp \
	rate_limiter.cpp \
	relation_full_handler.cpp \
	relations_handler.cpp \
	router.cpp \
	routes.cpp \
	way_full_handler.cpp \
	way_handler.cpp \
	ways_handler.cpp \
	writeable_pgsql_selection.cpp \
	xml_formatter.cpp \
	xml_writer.cpp \
	zlib.cpp

if HAVE_YAJL
___map_SOURCES+=json_formatter.cpp json_writer.cpp
endif
