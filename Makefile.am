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
	include/cgimap/types.hpp \
	include/cgimap/zlib.hpp

cgimap_api06_includedir=$(includedir)/cgimap/api06
cgimap_api06_include_HEADERS = \
	include/cgimap/api06/handler_utils.hpp \
	include/cgimap/api06/map_handler.hpp \
	include/cgimap/api06/node_handler.hpp \
	include/cgimap/api06/node_version_handler.hpp \
	include/cgimap/api06/nodes_handler.hpp \
	include/cgimap/api06/relation_full_handler.hpp \
	include/cgimap/api06/relation_handler.hpp \
	include/cgimap/api06/relation_version_handler.hpp \
	include/cgimap/api06/relations_handler.hpp \
	include/cgimap/api06/way_full_handler.hpp \
	include/cgimap/api06/way_handler.hpp \
	include/cgimap/api06/way_version_handler.hpp \
	include/cgimap/api06/ways_handler.hpp

if ENABLE_APIDB
cgimap_apidb_includedir=$(includedir)/cgimap/backend/apidb
cgimap_apidb_include_HEADERS = \
	include/cgimap/backend/apidb/apidb.hpp \
	include/cgimap/backend/apidb/cache.hpp \
	include/cgimap/backend/apidb/changeset.hpp \
	include/cgimap/backend/apidb/quad_tile.hpp \
	include/cgimap/backend/apidb/readonly_pgsql_selection.hpp \
	include/cgimap/backend/apidb/writeable_pgsql_selection.hpp \
	include/cgimap/backend/apidb/utils.hpp
endif

if ENABLE_PGSNAPSHOT
cgimap_pgsnapshot_includedir=$(includedir)/cgimap/backend/pgsnapshot
cgimap_pgsnapshot_include_HEADERS = \
	include/cgimap/backend/pgsnapshot/pgsnapshot.hpp \
	include/cgimap/backend/pgsnapshot/snapshot_selection.hpp
endif

cgimap_staticxml_includedir=$(includedir)/cgimap/backend/staticxml
cgimap_staticxml_include_HEADERS = \
	include/cgimap/backend/staticxml/staticxml.hpp

if ENABLE_EXPERIMENTAL
cgimap_api06_include_HEADERS += \
	include/cgimap/api06/node_ways_handler.hpp
endif

if ENABLE_API07
cgimap_api07_includedir=$(includedir)/cgimap/api07
cgimap_api07_include_HEADERS = \
	include/cgimap/api07/map_handler.hpp
endif

TESTS = test/map.testcore test/node.testcore test/anon.testcore test/way.testcore test/relation.testcore
TESTS += test/empty.testcore test/way_full.testcore test/relation_full.testcore
TESTS += test/test_parse_id_list test/test_oauth test/test_http test/test_parse_time
TESTS += test/changesets.testcore test/message.testcore test/routes.testcore
TESTS += test/redactions.testcore
if ENABLE_EXPERIMENTAL
TESTS += test/node_ways.testcore
endif
if HAVE_YAJL
TESTS += test/json.testcore
endif
if ENABLE_APIDB
TESTS += \
	test/test_apidb_backend_nodes \
	test/test_apidb_backend_oauth \
	test/test_apidb_backend_historic \
	test/test_apidb_backend_changesets \
	test/test_apidb_backend_changeset_downloads
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

man1_MANS = openstreetmap-cgimap.1
