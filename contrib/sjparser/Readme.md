[//]: # (start nodoc)
[![pipeline status](https://gitlab.com/dhurum/sjparser/badges/master/pipeline.svg)](https://gitlab.com/dhurum/sjparser/-/commits/master)
[![coverage report](https://gitlab.com/dhurum/sjparser/badges/master/coverage.svg)](https://gitlab.com/dhurum/sjparser/-/commits/master)
[![License](https://img.shields.io/:license-mit-blue.svg)](https://badges.mit-license.org)

[//]: # (end nodoc)
# SJParser

Streaming json parser, written on top of yajl.

The main use case for this parser is very long json documents with known structure, e.g. importing some data from a json representation, or reading a json from network and processing it chunk by chunk.

This is an event-driven parser on steroids - you specify the expected json structure and your callbacks, and they will be called after a whole piece of document is parsed (an object, for example), not just on `MapKey` or `ArrayEnd` events.

[//]: # (start nodoc)
## Documentation

[Class reference](https://dhurum.gitlab.io/sjparser/).

[Tutorial](https://gitlab.com/dhurum/sjparser_tutorial).

Also you can check the [Concepts](#concepts).

## Example

~~~cpp
#include <iostream>
#include "sjparser/sjparser.h"

using SJParser::Array;
using SJParser::Member;
using SJParser::Object;
using SJParser::Parser;
using SJParser::SArray;
using SJParser::Value;
using SJParser::Presence;

class TestPrinter {
 public:
  void writeObject(std::string str, int64_t integer,
                   std::vector<std::string> array) {
    std::cout << str << " " << integer << " [";

    const char *delimiter = "";
    for (const auto &elt : array) {
      std::cout << delimiter << elt;
      delimiter = ",";
    }
    std::cout << "]\n";
  }
};

TestPrinter DB;

int main() {
  /* Declare parser. It expects an array of objects, where first member is a
   * string, second member is an optional integer with default value 0, and
   * third member is an array of strings. We don't want to process individual
   * elements of the object's thied member, so we will use an array parser that
   * stores it's parsing result in an std::vector.
   */
  Parser parser{Array{Object{
      std::tuple{Member{"string", Value<std::string>{}},
                 Member{"integer", Value<int64_t>{}, Presence::Optional, 0},
                 Member{"array", SArray{Value<std::string>{}}}}}}};

  /* Callback, will be called once object is parsed.
   * It receives a reference to the object parser as an argument, so we use
   * parsers type aliases to get the type we need.
   */
  auto objectCb = [&](decltype(parser)::ParserType::ParserType &parser) {
    // Some external API call
    DB.writeObject(
        // Rvalue reference to the first object member (std::string)
        parser.pop<0>(),
        // Lvalue reference to the second object member (int64_t) or default
        // value if it is not present
        parser.get<1>(),
        // Rvalue reference to the third object member
        // (std::vector<std::string>)
        parser.pop<2>());
    // Returning false from the callback with make the parser stop with an error
    return true;
  };

  parser.parser().parser().setFinishCallback(objectCb);

  // Parse a piece of json. During parsing object callback will be called.
  parser.parse(R"(
  [{
    "string": "str1",
    "integer": 1,
    "array": ["1", "2"]
  }, {
    "string": "str2",
    "array": ["3", "4"]
  }])");

  // Finish parsing
  parser.finish();

  return 0;
}
~~~

For more examples, please see [tests](https://gitlab.com/dhurum/sjparser/tree/master/tests/src).

## Building

For building sjparser you will need:

- `cmake` 3.8 or higher;
- `make`;
- `yajl`;
- c++ compiler with c++17 support (should work with gcc 8 and clang 7);

### CMake variables:
- `SJPARSER_WITH_TESTS` - Build tests if the config is not Debug;
- `SJPARSER_WITH_COVERAGE` - Add coverage target (only in Debug config);
- `SJPARSER_BUILD_SHARED_LIBRARY` - Build shared library even in case of submodule build;

### Release build

~~~bash
mkdir build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
make
~~~

If you want to build tests for the release build you can use this command:

~~~bash
cmake ../ -DCMAKE_BUILD_TYPE=Release -DSJPARSER_WITH_TESTS=On
~~~

### Installation

~~~bash
make install
~~~

### Debug build

For the debug build you will need `gtest`.

~~~bash
cmake ../ -DCMAKE_BUILD_TYPE=Debug
~~~

### Running tests

~~~bash
make test
~~~

### Coverage

For the coverage you will need:

- `gcov`;
- `gcovr`;

~~~bash
cmake ../ -DCMAKE_BUILD_TYPE=Debug -DSJPARSER_WITH_COVERAGE=On
make
make coverage
~~~

The coverage report will be available in `coverage/report`.

### Static analyzer run

For the static analysis you will need `clang-tidy`.

~~~bash
make check
~~~

### Fix code formatting

For code formatting you will need `clang-format`.

~~~bash
make format
~~~

[//]: # (end nodoc)
## Concepts <a name="concepts"></a>

Expected json structure is specified as constructor arguments of `SJParser::Parser` and entities parsers.

Membersof `SJParser::Object`, `SJParser::SCustomObject` and `SJParser::SAutoObject` are mandatory.
You can pass `SJParser::Presence::Optional` to a `SJParser::Member` constructor to make the member optional.

Members of  `` in a standalone mode with a default value are not mandatory, even empty object would be successfully parsed.

If you call `get()` or `pop()` on a parser of an entity, that was not present in the parsed object, exception will be thrown.
You can check if member was parsed with method `isSet()`.
So, for your mandatory members you can just use `get()` or `pop()`, and for optional you can do checks with `isSet()` first.

For the `SJParser::Object` parsers there are two methods for accessing the members parsers:
 - `parser<n>()` - Returns a refecence to the n-th member parser;
 - `get<n>()` - If the n-th member stores parsed value (is  a `SJParser::Value`, `SJParser::SAutoObject`, `SJParser::SCustomObject` or `SJParser::SArray`), then a reference to a parsed value will be returned (or, if the entity was not present in the json - an exception will be thrown). Otherwise, a reference to the member parser will be returned;
 - `pop<n>()` - If the n-th member stores parsed value (is  a `SJParser::Value`, `SJParser::SAutoObject`, `SJPArser::SCustomObject` or `SJParser::SArray`), then an rvalue reference to a parsed value will be returned (or, if the entity was not present in the json - an exception will be thrown). Otherwise, this method is not defined;
