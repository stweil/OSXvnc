CC=cc
CXX=cc
CFLAGS=-O3 -Wall -ObjC
CXXFLAGS=-g -Wall
INCLUDES=-I. -Ilibvncauth -Iinclude -Iinclude/X11 -Iinclude/Xserver -Ilibjpeg
VNCAUTHLIB=-Llibvncauth -lvncauth 
SUPPORTLIB=-lz -Llibjpeg -ljpeg
RDRLIB=-Lrdr -lrdr
EXTRALIBS=-lstdc++

LIBS=$(VNCAUTHLIB) $(SUPPORTLIB) $(RDRLIB) $(EXTRALIBS) -framework Carbon -framework IOKit -framework Cocoa

SOURCES=main.c rfbserver.c miregion.c kbdptr.c auth.c sockets.c xalloc.c \
	stats.c corre.c hextile.c rre.c translate.c cutpaste.c dimming.c \
	tight.c zlib.c zlibhex.c localbuffer.c mousecursor.c zrle.cc 
OBJS=main.o rfbserver.o miregion.o kbdptr.o auth.o sockets.o xalloc.o \
	stats.o corre.o hextile.o rre.o translate.o cutpaste.o dimming.o \
	tight.o zlib.o zlibhex.o localbuffer.o mousecursor.o zrle.o VNCServer.o

all: OSXvnc-server storepasswd

install:

.m.o: 
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<
        
OSXvnc-server: $(OBJS) libvncauth/libvncauth.a libjpeg/libjpeg.a rdr/librdr.a
	$(CC) -o OSXvnc-server $(OBJS) $(LIBS) 

storepasswd: storepasswd.o libvncauth/libvncauth.a
	$(CC) -o storepasswd storepasswd.o $(VNCAUTHLIB)

libvncauth/libvncauth.a:
	(cd libvncauth; make)

libjpeg/libjpeg.a:
	(cd libjpeg; make libjpeg.a)

rdr/librdr.a:
	(cd rdr; make librdr.a)

clean:
	rm -f $(OBJS) *~ core "#"* *.bak *.orig *.o
	(cd libvncauth; make clean)
	(cd libjpeg; make clean)
	(cd rdr; make clean)

realclean: clean
	rm -f OSXvnc-server storepasswd .depend

depend:
	cc -M $(INCLUDES) $(SOURCES) > .depend

.depend:
	cc -M $(INCLUDES) $(SOURCES) > .depend

include .depend
