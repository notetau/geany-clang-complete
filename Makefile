
ifeq ($(PREFIX),)
INSTALL_DIR = `pkg-config --variable=libdir geany`/geany
else
INSTALL_DIR = $(PREFIX)
endif

LIBNAME = geanyclangcomplete.so

BASESRCS := base/cc_plugin.cpp base/suggestion_window.cpp base/completion_async.cpp

SRCS = preferences.cpp completion_framework.cpp completion.cpp plugin_info.cpp $(BASESRCS)

OBJS = $(addprefix lib/, $(SRCS:.cpp=.o))

CXXFLAGS += -fPIC `pkg-config --cflags geany` -O2 -std=c++11
LDFLAGS  += -shared `pkg-config --libs geany` -lclang


all: lib/$(LIBNAME)

lib/$(LIBNAME): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

lib/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@



install: lib/$(LIBNAME)
	cp lib/$(LIBNAME) $(INSTALL_DIR)/$(LIBNAME)

clean:
	rm lib/*.o
	rm lib/$(LIBNAME)
