VERSION=v2.0
BINDIR=/bin

CC = g++
CFLAGS  := -Wall -fPIC -std=c++11 -g -Wno-write-strings
CPPFLAGS := -Wall -fPIC  -std=c++11 -g -Wno-write-strings

RECORDSAVE= RecordSave/libRecordSave.so
BASE= Base/libbase.so

OPEN_LIB = -lcurl -lRecordSave -lglog -lpthread -lbase

MONITOR = recordMonitor
SERVER = recordServer

MOBJS = monitor.o
OBJS = mongoose.o webserver.o

TARGET = $(MONITOR) $(SERVER)

all : $(BASE) $(RECORDSAVE) $(TARGET)

install:	$(TARGET)
	cp $(TARGET) $(BINDIR)
	@cd Base; $(MAKE) install
	@cd RecordSave; $(MAKE) install

FORCE:

$(BASE): FORCE
	@cd Base; $(MAKE) all

$(RECORDSAVE): FORCE
	@cd RecordSave; $(MAKE) all

$(MONITOR) : $(MOBJS)
	$(CC) $(CPPFLAGS) -o $@ $(MOBJS) $(OPEN_LIB)

$(SERVER) : $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS) $(OPEN_LIB)

clean:
	rm -f $(OBJS) $(MOBJS) $(TARGET)
	@cd Base; $(MAKE) clean
	@cd RecordSave; $(MAKE) clean

webserver.o: webserver.cpp webserver.h
mongoose.o: mongoose.c mongoose.h
monitor.o: monitor.cpp
