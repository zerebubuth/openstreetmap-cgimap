ACLOCAL_AMFLAGS = -I aclocal
SUBDIRS=src

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

cgimap_includedir=$(includedir)/cgimap
cgimap_include_HEADERS = \
	include/cgimap/bbox.hpp \
	include/cgimap/backend.hpp \
	include/cgimap/config.hpp \
	include/cgimap/data_selection.hpp \
	include/cgimap/handler.hpp \
	include/cgimap/http.hpp \
	include/cgimap/logger.hpp \
	include/cgimap/mime_types.hpp \
	include/cgimap/output_buffer.hpp \
	include/cgimap/output_formatter.hpp \
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

if HAVE_CLANG_FORMAT
.PHONY: clang-format
clang-format:
	@echo "Reformatting files..."; \
	for i in include/api06/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/cgimap/api06/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/cgimap/api07/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/cgimap/backend/apidb/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/cgimap/backend/pgsnapshot/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/cgimap/backend/staticxml/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/cgimap/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in include/*.hpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in src/api06/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in src/api07/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in src/backend/apidb/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in src/backend/pgsnapshot/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in src/backend/staticxml/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in src/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done; \
	for i in test/*.cpp; do @CLANG_FORMAT@ -style=file -i $$i; done;
endif
