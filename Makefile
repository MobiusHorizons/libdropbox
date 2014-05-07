CCFLAGS := -ggdb
ifeq ($(CC),cc)
SL :=.so
CCFLAGS := $(CCFLAGS) -fPIC
PKG_CONFIG :=pkg-config
else
SL:=.dll
EXE:=.exe
#CCFLAGS := $(CCFLAGS) -DCURL_STATICLIB
#LIBS := $(shell $(PKG_CONFIG) --libs libcurl)
endif
all: dropbox$(EXE) libdropbox$(SL)

dropbox: dropbox_api.o dropbox_main.o
	$(CC) $(CCFLAGS) -o dropbox dropbox_api.o dropbox_main.o -pthread -ljson-c -lcurl

dropbox.exe: dropbox_api.o dropbox_main.o
	$(CC) $(CCFLAGS) -o dropbox.exe dropbox_api.o dropbox_main.o -pthread -ljson-c -lcurl -lws2_32

#dropbox.exe: dapi.o dropbox_main.o
#	$(CC) $(CCFLAGS) -o dropbox.exe dropbox_api.o dropbox_main.o -ljson-c -pthread $(LIBS)

libdropbox.so: dropbox_api.o
	$(LD) -shared -o libdropbox.so dropbox_api.o  

libdropbox.dll: dropbox_api.o
	$(CC) -shared -o $@ $< -static -ljson-c -lpthread -Wl,--out-implib,$@.a,-Bdynamic -lws2_32 -lcurl

dropbox_api.o: dropbox_api.c dropbox_api.h dropbox_urls.h buffer.h
	$(CC) $(CCFLAGS) -c dropbox_api.c

dropbox_main.o: dropbox_main.c dropbox_api.h
	$(CC) $(CCFLAGS) -c dropbox_main.c 
