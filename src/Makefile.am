LDADD=@LIBPQXX_LIBS@ @FCGI_LIBS@ @YAJL_LIBS@ @LIBXML_LIBS@ @LIBMEMCACHED_LIBS@ @BOOST_REGEX_LIB@ @BOOST_PROGRAM_OPTIONS_LIB@ @BOOST_DATE_TIME_LIB@
AM_LDFLAGS=@BOOST_LDFLAGS@
AM_CPPFLAGS=-I../include @FCGI_CFLAGS@ @LIBPQXX_CFLAGS@ @LIBXML_CFLAGS@ @LIBMEMCACHED_CFLAGS@ @BOOST_CPPFLAGS@

bin_PROGRAMS=../map
___map_SOURCES=bbox.cpp http.cpp main.cpp quad_tile.cpp \
	xml_writer.cpp changeset.cpp \
	logger.cpp zlib.cpp xml_formatter.cpp \
	output_formatter.cpp handler.cpp map_handler.cpp \
	fcgi_helpers.cpp output_writer.cpp routes.cpp \
	osm_helpers.cpp rate_limiter.cpp router.cpp \
	node_handler.cpp nodes_handler.cpp choose_formatter.cpp \
	mime_types.cpp osm_responder.cpp relations_handler.cpp\
	relation_full_handler.cpp way_handler.cpp ways_handler.cpp \
	way_full_handler.cpp

if HAVE_YAJL
___map_SOURCES+=json_formatter.cpp json_writer.cpp
endif
