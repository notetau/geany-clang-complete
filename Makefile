
ifeq ($(PREFIX),)
INSTALL_DIR = `pkg-config --variable=libdir geany`/geany
else
INSTALL_DIR = $(PREFIX)
endif


LIBNAME = geanyclangcomplete.so

SRCS = cc_plugin.cpp completion.cpp ui.cpp preferences.cpp
OBJS = $(addprefix lib/, $(SRCS:.cpp=.o))

CXXFLAGS += -fPIC `pkg-config --cflags geany` -O2
LDFLAGS  += -shared `pkg-config --libs geany` -lclang


all: lib/$(LIBNAME)

lib/$(LIBNAME): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

lib/%.o: src/%.cpp
	$(CXX) -c $< $(CXXFLAGS) -o $@


install: lib/$(LIBNAME)
	cp lib/$(LIBNAME) $(INSTALL_DIR)/$(LIBNAME)

clean:
	rm lib/*.o
	rm lib/$(LIBNAME)
