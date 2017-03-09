CFLAGS=-g -std=gnu99 -Iinclude
LDFLAGS=-g

SRCS = $(wildcard src/*.c)
HEADERS = $(wildcard include/*.h)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# instruct GCC to produce dependency files
CFLAGS += -MMD -MP

SUBDIRS=examples
.PHONY: $(SUBDIRS)

all: liblaik.a $(SUBDIRS)

liblaik.a: $(OBJS)
	ar rcs liblaik.a $(OBJS)

examples: liblaik.a
	cd examples && $(MAKE)

clean:
	rm -f *~ *.o $(OBJS) $(DEPS) liblaik.a
	cd examples && make clean

# include previously generated dependency rules if existing
-include $(DEPS)