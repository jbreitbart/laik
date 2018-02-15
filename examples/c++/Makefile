# default settings
OPT = -g

# settings from 'configure', may overwrite defaults
-include ../../Makefile.config

EXAMPLES = raytracer

CXXFLAGS = $(OPT) $(WARN) $(DEFS) -I$(SDIR)../../include
LAIKLIB = $(abspath ../../liblaik.so)
LDFLAGS=$(OPT) $(LAIKLIB) -lGPI2-dbg -L/home/jbreitbart/bin/gpi2/lib64 -pthread

all: $(EXAMPLES)

raytracer: $(SDIR)raytracer.cpp $(LAIKLIB)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f *.o *~ *.ppm $(EXAMPLES)
