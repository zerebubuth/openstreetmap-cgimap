LDADD=@LIBPQXX_LIBS@ @FCGI_LIBS@ @LIBXML_LIBS@ @BOOST_REGEX_LIB@ @BOOST_PROGRAM_OPTIONS_LIB@
AM_LDFLAGS=@BOOST_LDFLAGS@
AM_CPPFLAGS=-I../include @FCGI_CFLAGS@ @LIBPQXX_CFLAGS@ @LIBXML_CFLAGS@ @BOOST_CPPFLAGS@

bin_PROGRAMS=../map
___map_SOURCES=bbox.cpp http.cpp main.cpp map.cpp quad_tile.cpp \
	split_tags.cpp temp_tables.cpp writer.cpp changeset.cpp \
	logger.cpp zlib.cpp

