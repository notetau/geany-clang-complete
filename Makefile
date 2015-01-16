
ifeq ($(PREFIX),)
INSTALL_DIR = `pkg-config --variable=libdir geany`/geany
else
INSTALL_DIR = $(PREFIX)
endif


LIBNAME = geanyclangcomplete.so

SRCS = cc_plugin.cpp suggestion_window.cpp  completion_async.cpp \
cpp/completion.cpp cpp/completion_framework.cpp cpp/preferences.cpp
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
