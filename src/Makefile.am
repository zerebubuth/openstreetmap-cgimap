LDADD=@LIBPQXX_LIBS@ @FCGI_LIBS@ @YAJL_LIBS@ @LIBXML_LIBS@ @BOOST_REGEX_LIB@ @BOOST_PROGRAM_OPTIONS_LIB@
AM_LDFLAGS=@BOOST_LDFLAGS@
AM_CPPFLAGS=-I../include @FCGI_CFLAGS@ @LIBPQXX_CFLAGS@ @LIBXML_CFLAGS@ @BOOST_CPPFLAGS@

bin_PROGRAMS=../map
___map_SOURCES=bbox.cpp http.cpp main.cpp map.cpp quad_tile.cpp \
	split_tags.cpp temp_tables.cpp writer.cpp changeset.cpp \
	logger.cpp zlib.cpp xml_formatter.cpp \
	output_formatter.cpp handler.cpp map_handler.cpp \
	fcgi_helpers.cpp output_writer.cpp routes.cpp

if HAVE_YAJL
___map_SOURCES+=json_formatter.cpp json_writer.cpp
endif
