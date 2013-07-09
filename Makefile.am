SUBDIRS=src

TESTS = 
if ENABLE_APIDB
TESTS += test/test_empty.rb test/test_map.rb test/test_anon.rb
if ENABLE_EXPERIMENTAL
TESTS += test/test_node.rb test/test_way.rb test/test_relation.rb
endif
endif
TEST_EXTENSIONS = .rb
RB_LOG_COMPILER = test/test.sh
