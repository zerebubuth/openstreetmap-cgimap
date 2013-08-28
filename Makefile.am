SUBDIRS=src

TESTS = 
if ENABLE_APIDB
TESTS += test/test_empty.rb test/test_map.rb test/test_anon.rb
TESTS += test/test_node.rb test/test_way.rb test/test_relation.rb
TESTS += test/test_way_full.rb test/test_relation_full.rb
endif
TEST_EXTENSIONS = .rb
RB_LOG_COMPILER = test/test.sh
