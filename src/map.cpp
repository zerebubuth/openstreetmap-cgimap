#include "map.hpp"
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <pqxx/pqxx>

#include "temp_tables.hpp"
#include "split_tags.hpp"
#include "cache.hpp"
#include "logger.hpp"

using std::vector;
using std::string;
using std::transform;
using boost::shared_ptr;


