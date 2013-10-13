SUBDIRS=src

TESTS = test/map.testcore
if ENABLE_APIDB
TESTS += test/test_empty.rb test/test_map.rb test/test_anon.rb
TESTS += test/test_node.rb test/test_way.rb test/test_relation.rb
TESTS += test/test_way_full.rb test/test_relation_full.rb
if ENABLE_EXPERIMENTAL
TESTS += test/test_node_ways.rb
endif
if HAVE_YAJL
TESTS += test/test_json.rb
endif
endif
TEST_EXTENSIONS = .rb .testcore
RB_LOG_COMPILER = test/test.sh
TESTCORE_LOG_COMPILER = test/test_core
