ACLOCAL_AMFLAGS = -I aclocal
SUBDIRS=src

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

cgimap_includedir=$(includedir)/cgimap
cgimap_include_HEADERS = \
	include/cgimap/backend.hpp \
	include/cgimap/data_selection.hpp \
	include/cgimap/handler.hpp \
	include/cgimap/http.hpp \
	include/cgimap/logger.hpp \
	include/cgimap/process_request.hpp \
	include/cgimap/rate_limiter.hpp \
	include/cgimap/request_helpers.hpp \
	include/cgimap/request.hpp \
	include/cgimap/routes.hpp \
	include/cgimap/types.hpp

TESTS = test/map.testcore test/node.testcore test/anon.testcore test/way.testcore test/relation.testcore
TESTS += test/empty.testcore test/way_full.testcore test/relation_full.testcore
if ENABLE_EXPERIMENTAL
TESTS += test/node_ways.testcore
endif
if HAVE_YAJL
TESTS += test/json.testcore
endif
if ENABLE_APIDB
TESTS += test/test_apidb_backend
endif
TEST_EXTENSIONS = .testcore
TESTCORE_LOG_COMPILER = test/test_core
