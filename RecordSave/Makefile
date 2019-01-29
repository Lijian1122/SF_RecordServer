VERSION=v2.4

prefix=/usr/local

CC=$(CROSS_COMPILE)g++
#@gcc
LD=$(CROSS_COMPILE)ld

SYS=posix
#SYS=mingw

CRYPTO=OPENSSL
#CRYPTO=POLARSSL
#CRYPTO=GNUTLS
LIBZ=-lz
LIB_GNUTLS=-lgnutls -lhogweed -lnettle -lgmp $(LIBZ)
LIB_OPENSSL=-lssl -lcrypto $(LIBZ)
LIB_POLARSSL=-lpolarssl $(LIBZ)
CRYPTO_LIB=$(LIB_$(CRYPTO))
DEF_=-DNO_CRYPTO
CRYPTO_DEF=$(DEF_$(CRYPTO))

DEF=-DRTMPDUMP_VERSION=\"$(VERSION)\" $(CRYPTO_DEF) $(XDEF)
#OPT=-O2 -g -std=gnu++11 -std=c++11
CFLAGS= -g -Wall -fPIC -Wno-error=overflow $(XCFLAGS) $(INC) $(DEF) $(OPT)
CPPFLAGS += -g -fPIC -std=c++11
LDFLAGS=-Wall $(XLDFLAGS)

SHARE := -shared

bindir=$(prefix)/bin
sbindir=$(prefix)/sbin
mandir=$(prefix)/man
libdir=/lib

BINDIR=$(DESTDIR)$(bindir)
SBINDIR=$(DESTDIR)$(sbindir)
MANDIR=$(DESTDIR)$(mandir)

LIBS_posix=
LIBS_darwin=
LIBS_mingw=-lws2_32 -lwinmm -lgdi32
LIB_RTMP=-Llibrtmp -lrtmp -lglog -lpthread
LIBS=$(LIB_RTMP) $(CRYPTO_LIB) $(LIBS_$(SYS)) $(XLIBS)

THREADLIB_posix=-lpthread
THREADLIB_darwin=-lpthread
THREADLIB_mingw=
THREADLIB=$(THREADLIB_$(SYS))
SLIBS=$(THREADLIB) $(LIBS)

LIBRTMP=librtmp/librtmp.a
INCRTMP=librtmp/rtmp_sys.h librtmp/rtmp.h librtmp/log.h librtmp/amf.h ../Base/LibcurClient.h CCycleBuffer.h


PROGS= libRecordSave.so
OBJS = CCycleBuffer.o RecordSaveRunnable.o
   
all:	$(LIBRTMP) $(PROGS)

$(PROGS): $(LIBRTMP)

install:	$(PROGS)
	-mkdir -p $(BINDIR) $(SBINDIR)   
	cp $(PROGS) $(libdir)
	@cd librtmp; $(MAKE) install

clean:
	rm -f *.o $(PROGS)
	@cd librtmp; $(MAKE) clean

FORCE:

$(LIBRTMP): FORCE
	@cd librtmp; $(MAKE) all

$(PROGS): $(OBJS)
	$(CC) $(LDFLAGS) $(SHARE) -o $@ $(OBJS)  $(LIBS) 

#rtmpdump.o: rtmpdump.c $(INCRTMP) Makefile
#thread.o: thread.c $(INCRTMP) thread.h

#Makefile
RecordRunnable.o: RecordSaveRunnable.cpp $(INCRTMP) RecordSaveRunnable.h   
CCycleBuffer.o: CCycleBuffer.cpp $(INCRTMP) CCycleBuffer.h
