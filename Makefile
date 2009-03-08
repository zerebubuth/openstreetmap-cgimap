CXX=g++ -g -ggdb
CFLAGS=\
	$(shell pkg-config --cflags libxml-2.0) \
	-I/usr/include/mysql \
	-I/usr/include/mysql++ \
	-I$(HOME)/include/mysql++ \
	-Iinclude

LIBS=\
	$(shell pkg-config --libs libxml-2.0) \
	-L$(HOME)/lib -lmysqlpp -lfcgi

SRCS=\
	bbox.cpp \
	http.cpp \
	main.cpp \
	map.cpp \
	quad_tile.cpp \
	split_tags.cpp \
	temp_tables.cpp \
	writer.cpp

OBJS=$(patsubst %.cpp,obj/%.o,$(SRCS))
DEPS=$(patsubst %.cpp,dep/%.d,$(SRCS))

map: $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

obj/%.o: %.cpp %.d obj
	$(CXX) $(CFLAGS) -c $< -o $@

dep/%.d: %.cpp dep
	$(CXX) $(CFLAGS) -M -MM -o $@ -c $<

clean: 
	rm -rf map obj dep

obj: 
	mkdir obj

dep:
	mkdir dep

vpath %.hpp include
vpath %.cpp src
vpath %.o   obj
vpath %.d   dep

ifneq  ($(MAKECMDGOALS),clean)
include $(DEPS)
endif

