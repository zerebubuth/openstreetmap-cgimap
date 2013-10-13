#include "process_request.hpp"
#include "backend/staticxml/staticxml.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <vector>

namespace fs = boost::filesystem;
namespace po = boost::program_options;

void run_test(boost::shared_ptr<data_selection::factory> factory, fs::path test_case) {
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <test-directory>." << std::endl;
    return 99;
  }

  fs::path test_directory = argv[1];
  fs::path data_file = test_directory / "data.osm";
  std::vector<fs::path> test_cases;

  try {
    if (fs::is_directory(test_directory) == false) {
      std::cerr << "Test directory " << test_directory << " should be a directory, but isn't.";
      return 99;
    }
    if (fs::is_regular_file(data_file) == false) {
      std::cerr << "Test directory should contain data file at " << data_file << ", but does not.";
      return 99;
    }
    BOOST_FOREACH(fs::path filename, test_directory) {
      if (fs::extension(filename) == ".case") {
        test_cases.push_back(filename);
      }
    }

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 99;
  
  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 99;
  }

  try {
    po::variables_map vm;
    vm.insert(std::make_pair(std::string("file"), po::variable_value(data_file.native(), false)));

    boost::shared_ptr<backend> data_backend = make_staticxml_backend();
    boost::shared_ptr<data_selection::factory> factory = data_backend->create(vm);

    BOOST_FOREACH(fs::path test_case, test_cases) {
      run_test(factory, test_case);
    }

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  
  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}
