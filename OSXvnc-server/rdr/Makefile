
SRCS = FdInStream.cxx FdOutStream.cxx InStream.cxx NullOutStream.cxx \
       ZlibInStream.cxx ZlibOutStream.cxx

OBJS = $(SRCS:.cxx=.o)

DIR_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/zlib

library = librdr.a

all:: $(library)

$(library): $(OBJS)
	rm -f $(library)
	$(AR) $(library) $(OBJS)
	$(RANLIB) $(library)

# followed by boilerplate.mk

all::
	@subdirs="$(SUBDIRS)"; for d in $$subdirs; do (cd $$d; $(MAKE) $@) || exit 1; done

clean::
	@subdirs="$(SUBDIRS)"; for d in $$subdirs; do (cd $$d; $(MAKE) $@) || exit 1; done

clean::
	rm -f $(program) $(library) *.o

SHELL = /bin/sh
top_srcdir = ..

CC = gcc
CFLAGS = -O2 -Wall -no-cpp-precomp $(DIR_CFLAGS)
CCLD = $(CC)
CXX = g++
CXXFLAGS = -O2 -Wall
CXXLD = $(CXX)
CPPFLAGS = 
DEFS = 
ALL_CPPFLAGS = $(CPPFLAGS) $(DEFS) $(DIR_CPPFLAGS)
LIBS = 
LDFLAGS = 
RANLIB = ranlib
AR = ar cq

.SUFFIXES:
.SUFFIXES: .cxx .c .o

.c.o:
	$(CC) $(ALL_CPPFLAGS) $(CFLAGS) -c $<

.cxx.o:
	$(CXX) $(ALL_CPPFLAGS) $(CXXFLAGS) -c $<
