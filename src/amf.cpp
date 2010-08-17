#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <utility>
#include <boost/date_time.hpp>
#include <arpa/inet.h>

using std::map;
using std::vector;
using std::string;
using std::ostream;
using std::cout;
using std::make_pair;
namespace pt = boost::posix_time;

/*********************************************************************
 * work in process! this isn't currently used, but hopefully will be *
 * until then, it's just going to sit here! ;-)                      *
 ********************************************************************/

#define AMF_FIRST_INDEX 0

namespace amf3 {
enum marker {
  marker_undefined  = 0x00,
  marker_null       = 0x01,
  marker_false      = 0x02,
  marker_true       = 0x03,
  marker_integer    = 0x04,
  marker_double     = 0x05,
  marker_string     = 0x06,
  marker_xml_doc    = 0x07,
  marker_date       = 0x08,
  marker_array      = 0x09,
  marker_object     = 0x0A,
  marker_xml        = 0x0B,
  marker_byte_array = 0x0C
};

class stream {
private:
  const pt::ptime time_t_epoch;

  ostream &out;
  
  typedef map<pt::ptime,int> date_map_t;
  typedef map<vector<int>,int> int_array_map_t;
  typedef map<map<string,string>,int> assoc_array_map_t;
  typedef map<string, int> string_map_t;

  date_map_t date_table;
  int_array_map_t int_array_table;
  assoc_array_map_t assoc_array_table;
  string_map_t string_table;

  int date_table_counter, int_array_table_counter, array_table_counter, string_table_counter;

public:

  stream(ostream &o) 
    : out(o),
      date_table_counter(AMF_FIRST_INDEX),
      int_array_table_counter(AMF_FIRST_INDEX),
      array_table_counter(AMF_FIRST_INDEX),
      string_table_counter(AMF_FIRST_INDEX),
      time_t_epoch(boost::gregorian::date(1970,1,1)) {
  }

  stream &operator<<(bool b) {
    out << (char)(b ? marker_true : marker_false);
    return *this;
  }

  stream &operator<<(double d) {
    union {
      double d;
      uint64_t u64;
      uint8_t array[8];
    } host, network;

    host.d = d;
    network.u64 = htobe64(host.u64);

    out << (char)marker_double;
    for (int i = 0; i < 8; ++i)
      out << (char)network.array[i];

    return *this;
  }

  stream &operator<<(int i) {
    if (i >= 0x40000000 || (-i) >= 0x40000000) {
      // values this large must be encoded as doubles instead
      operator<<((double)i);

    } else {
      out << (char)marker_integer;
      if (i > 0x1fffff) { 
	out << (char)(0x80 | ((i >> 21) & 0xff)); 
	out << (char)(0x80 | ((i >> 14) & 0xff)); 
	out << (char)(0x80 | ((i >>  7) & 0xff)); 
	out << (char)(0x80 | (i & 0xff)); 
      } else if (i > 0x3fff) {
	out << (char)(0x80 | ((i >> 14) & 0xff)); 
	out << (char)(0x80 | ((i >>  7) & 0xff)); 
	out << (char)(0x80 | (i & 0xff)); 
      } else if (i > 0x7f) {
	out << (char)(0x80 | ((i >>  7) & 0xff)); 
	out << (char)(0x80 | (i & 0xff)); 
      } else {
	out << (char)(0x80 | (i & 0xff)); 
      }
    }
    return *this;
  }

  stream &operator<<(const string &s) {
    out << (char)marker_string;
    output_utf8_vr(s);
    return *this;
  }

  stream &operator<<(const pt::ptime &t) {
    out << (char)marker_date;
    date_map_t::const_iterator itr = date_table.find(t);
    if (itr == date_table.end()) {
      date_table.insert(make_pair(t, date_table_counter++));
      operator<<((int)1);
      operator<<((double)((t - time_t_epoch).total_milliseconds()));

    } else {
      operator<<((int)(itr->second << 1));
    }
    return *this;
  }

  stream &operator<<(const vector<int> &a) {
    out << (char)marker_array;
    int_array_map_t::const_iterator itr = int_array_table.find(a);
    if (itr == int_array_table.end()) {
      int_array_table.insert(make_pair(a, array_table_counter++));
      operator<<((int)((a.size() << 1) | 1));
      operator<<((int)1); // empty string marker - there are no associative values here
      for (vector<int>::const_iterator jtr = a.begin(); jtr != a.end(); ++jtr) {
	out << *jtr;
      }

    } else {
      operator<<((int)(itr->second << 1));
    }
    return *this;
  }

  stream &operator<<(const map<string, string> &m) {
    out << (char)marker_array;
    assoc_array_map_t::const_iterator itr = assoc_array_table.find(m);
    if (itr == assoc_array_table.end()) {
      assoc_array_table.insert(make_pair(m, array_table_counter++));
      operator<<((int)1); // size of dense portion is zero
      for (map<string, string>::const_iterator jtr = m.begin(); jtr != m.end(); ++jtr) {
	// doesn't look like we can encode the empty string as a key, since that's
	// the end-terminator for associative pairs...
	if (!jtr->first.empty()) {
	  output_utf8_vr(jtr->first);
	  output_utf8_vr(jtr->second);
	}
      }

    } else {
      operator<<((int)(itr->second << 1));
    }
    return *this;
  }

private:
  void output_utf8_vr(const string &s) {
    if (s.empty()) {
      // the empty string has a special encoding
      operator<<((int)1);

    } else {
      string_map_t::const_iterator itr = string_table.find(s);
      if (itr == string_table.end()) {
	string_table.insert(make_pair(s, string_table_counter++));
	operator<<((int)((s.size() << 1) | 1));
	out << s;
	
      } else {
	operator<<((int)(itr->second << 1));
      }
    }
  }
      
};

}

namespace amf0 {
enum marker {
  marker_number = 0x00,
  marker_bool = 0x01,
  marker_string = 0x02,
  marker_object = 0x03,
  marker_hash = 0x08,
  marker_object_end = 0x09,
  marker_strict_array = 0x0a,
  marker_date = 0x0b,
  marker_long_string = 0x0c
};

class stream {
private:
  const pt::ptime time_t_epoch;

  ostream &out;

  void out_marker(marker m) {
    out_int8(m);
  }

  void out_int8(char c) {
    out << c;
  }

  void out_int16(int i) {
    union {
      int16_t i;
      uint16_t ui;
      char a[2];
    } host, network;

    host.i = i;
    network.ui = htons(host.ui);

    out_int8(network.a[0]);
    out_int8(network.a[1]);
  }

  void out_int32(int i) {
    union {
      int32_t i;
      uint32_t ui;
      char a[4];
    } host, network;

    host.i = i;
    network.ui = htonl(host.ui);
    
    out_int8(network.a[0]);
    out_int8(network.a[1]);
    out_int8(network.a[2]);
    out_int8(network.a[3]);
  }

  void out_double(double d) {
    union {
      double d;
      uint64_t u64;
      uint8_t array[8];
    } host, network;

    host.d = d;
    network.u64 = htobe64(host.u64);

    for (int i = 0; i < 8; ++i)
      out_int8(network.array[i]);
  }

  void out_string(const string &s) {
    for (string::const_iterator itr = s.begin(); itr != s.end(); ++itr) {
      out_int8(*itr);
    }
  }

public:
  stream(ostream &o) 
    : out(o),
      time_t_epoch(boost::gregorian::date(1970,1,1)) {
  }

  stream &operator<<(bool b) {
    out_marker(marker_bool);
    out_int8(b ? 1 : 0);
    return *this;
  }

  stream &operator<<(double d) {
    out_marker(marker_number);
    out_double(d);
    return *this;
  }

  stream &operator<<(const string &s) {
    if (s.size() > (1 << 16) - 1) {
      out_marker(marker_long_string);
      out_int32(s.size());
    } else {
      out_marker(marker_string);
      out_int16(s.size());
    }
    out_string(s);
    return *this;
  }

  stream &operator<<(pt::ptime &t) {
    out_marker(marker_date);
    out_double((t - time_t_epoch).total_milliseconds());
    out_int16(0);
    return *this;
  }

  stream &operator<<(const vector<int> &v) {
    out_marker(marker_strict_array);
    out_int32(v.size());
    for (vector<int>::const_iterator itr = v.begin(); itr != v.end(); ++itr) {
      // in amf0 all numbers are written as doubles
      *this << (double)(*itr);
    }
    return *this;
  }

  stream &operator<<(const map<string,string> &m) {
    out_marker(marker_hash);
    out_int32(m.size());
    for (map<string,string>::const_iterator itr = m.begin(); itr != m.end(); ++itr) {
      out_int16(itr->first.size());
      out_string(itr->first);
      *this << itr->second;
    }
    out_int16(0);
    out_marker(marker_object_end);
    return *this;
  }

  void start_object() {
    out_marker(marker_object);
  }

  stream &object_key(const string &s) {
    out_int16(s.size());
    out_string(s);
    return *this;
  }

  void end_object() {
    out_int16(0);
    out_marker(marker_object_end);
  }

  void start_array(size_t num_elements) {
    out_marker(marker_strict_array);
    out_int32(v.size());
  }

  void end_array() {
  }
};
}

int
main(int argc, char *argv[]) {

  map<string,string> a_hash;
  a_hash["a"] = "blah";
  a_hash["b"] = "bloog";
  a_hash["c"] = "hooble";
  
  vector<int> an_array;
  an_array.push_back(1);
  an_array.push_back(2);
  an_array.push_back(3);
  an_array.push_back(4);
  an_array.push_back(5);
  an_array.push_back(6);

  amf0::stream out(cout);

  out.start_object();
  out.object_key("nodes") << a_hash;
  out.object_key("an_array") << an_array;
  out.object_key("foo") << string("bar");
  out.end_object();
    
  return 0;
}
