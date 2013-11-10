SUBDIRS=src

TESTS = test/map.testcore test/node.testcore test/anon.testcore test/way.testcore test/relation.testcore
TESTS += test/empty.testcore test/way_full.testcore test/relation_full.testcore
if ENABLE_EXPERIMENTAL
TESTS += test/node_ways.testcore
endif
if HAVE_YAJL
TESTS += test/json.testcore
endif
TEST_EXTENSIONS = .testcore
TESTCORE_LOG_COMPILER = test/test_core
