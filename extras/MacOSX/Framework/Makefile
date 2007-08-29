CC=gcc
CFLAGS=-g -O2
OBJCFLAGS=-fobjc-exceptions 
LDFLAGS=-single_module -read_only_relocs suppress

# We should set this properly.
srcdir=../../..
LIBVLC=$(srcdir)/src/.libs/libvlc.1.dylib $(srcdir)/src/.libs/libvlc-control.0.dylib
LIBVLC_HEADERS=$(srcdir)/include
VLCCONFIG=$(srcdir)/vlc-config

MODULES = $(patsubst %,$(SRC_DIR)/%,$(_MODULES))

HEADERS_DIR = Headers

_EXPORTED_HEADERS= \
     VLC/VLC.h \
     VLC/VLCMedia.h \
     VLC/VLCMediaDiscoverer.h \
     VLC/VLCMediaLibrary.h \
     VLC/VLCPlaylist.h \
     VLC/VLCPlaylistDataSource.h \
     VLC/VLCServicesDiscoverer.h \
     VLC/VLCTime.h \
     VLC/VLCVideoView.h

EXPORTED_HEADERS = $(patsubst %,$(HEADERS_DIR)/%,$(_EXPORTED_HEADERS))

EXPORTED_RESOURCES= \
     Resources/Info.plist \
     Resources/version.plist

SRC_DIR = Sources

_SRC = \
     test.m \
     VLCEventManager.m \
     VLCLibrary.m \
	 VLCMedia.m \
	 VLCMediaLibrary.m \
     VLCMediaDiscoverer.m \
	 VLCPlaylist.m \
     VLCPlaylistDataSource.m \
     VLCServicesDiscoverer.m \
     VLCTime.m \
     VLCVideoView.m

SRC = $(patsubst %,$(SRC_DIR)/%,$(_SRC))

HEADERS = $(EXPORTED_HEADERS)

INCLUDES=  -I .  -I $(LIBVLC_HEADERS) -I $(HEADERS_DIR)

FRAMEWORKS= -framework Cocoa  

OBJECTS=$(SRC:.m=.o)

all: VLC.framework

$(OBJECTS): $(HEADERS)

.m.o: $<
	$(CC) -c $(CFLAGS) $(OBJCFLAGS) $(INCLUDES) $< -o $@

DIR = VLC.framework \
      VLC.framework/Version/Current/Framework \
      VLC.framework/Version/Current/Headers \

VLC.framework/lib/libvlc.dylib: $(srcdir)/src/.libs/libvlc.dylib VLC.framework/lib
	cp -f $(srcdir)/src/.libs/libvlc.1.dylib VLC.framework/lib/libvlc.dylib && \
	install_name_tool -id `pwd`/VLC.framework/lib/libvlc.1.dylib \
	                   VLC.framework/lib/libvlc.dylib

VLC.framework/lib/libvlc-control.dylib: $(srcdir)/src/.libs/libvlc-control.dylib VLC.framework/lib
	mkdir -p VLC.framework/Version/Current/lib && \
	cp -f $< $@ && \
	install_name_tool -id `pwd`/$@ $@ && \
	install_name_tool -change /usr/local/lib/libvlc.1.dylib \
	                          `pwd`/VLC.framework/lib/libvlc.dylib  $@


VLC.framework/Headers: $(HEADERS)
	mkdir -p VLC.framework/Version/Current/Headers && \
	cp -f $(EXPORTED_HEADERS) VLC.framework/Version/Current/Headers && \
	ln -sf Version/Current/Headers VLC.framework

VLC.framework/Resources:
	mkdir -p VLC.framework/Version/Current/Resources && \
	cp -f $(EXPORTED_RESOURCES) VLC.framework/Version/Current/Resources && \
	ln -sf Version/Current/Resources VLC.framework


VLC.framework/modules:
	/usr/bin/install -c -d ./VLC.framework/Version/Current/modules && \
	for i in `top_builddir="$(srcdir)" $(VLCCONFIG) --target plugin` ; do \
	  if test -n "$$i" ; \
        then \
	    cp "`pwd`/`dirname $$i`/.libs/`basename $$i`.dylib" \
	       "./VLC.framework/Version/Current/modules" ; \
		module="./VLC.framework/Version/Current/modules/`basename $$i`.dylib"; \
	    install_name_tool -change /usr/local/lib/libvlc.1.dylib \
                                  @loader_path/../lib/libvlc.dylib \
                          "$$module"; \
	    echo "changing install name of $$module";\
	    for lib in `otool -L "$$module" | grep @executable_path | sed 's/(\([0-z]*\ *\.*\,*\)*)//g'` ; do \
	        install_name_tool -change "$$lib" \
                                       `echo "$$lib" | sed 's:executable_path:loader_path/../:'` \
                              "$$module"; \
	    done; \
	  fi \
    done && \
	ln -sf Version/Current/modules VLC.framework
	

VLC.framework/share:
	cp -R $(srcdir)/share ./VLC.framework/Version/Current && \
	ln -sf Version/Current/share ./VLC.framework

VLC.framework/lib: 
	mkdir -p VLC.framework/Version/Current/lib && \
	if test -d $(srcdir)/extras/contrib/vlc-lib; then \
	  for i in $(srcdir)/extras/contrib/vlc-lib/*.dylib ; do \
		module="VLC.framework/Version/Current/lib/`basename $${i}`"; \
	    cp `pwd`/$${i}  $${module} ; \
		install_name_tool -change /usr/local/lib/libvlc.1 @loader_path/../lib/libvlc.dylib \
		                  $${module}; \
	    echo "changing install name of $$module";\
	    for lib in `otool -L "$$module" | grep @executable_path | sed 's/(\([0-z]*\ *\.*\,*\)*)//g'` ; do \
	        install_name_tool -change "$$lib" \
                                       `echo "$$lib" | sed 's:executable_path:loader_path/../:'` \
                              "$$module"; \
	    done; \
	  done \
    fi && \
	ln -sf Version/Current/lib VLC.framework
    
VLC.framework/VLC:
	ln -sf Version/Current/VLC VLC.framework

VLC.framework/Version/Current/VLC: $(OBJECTS) $(LIBVLC) VLC.framework/Headers VLC.framework/Resources VLC.framework/lib/libvlc-control.dylib VLC.framework/lib/libvlc.dylib VLC.framework/modules VLC.framework/share VLC.framework/VLC
	mkdir -p VLC.framework/Version/Current/Framework && \
	$(CXX) -dynamiclib $(LDFLAGS) $(OBJECTS) $(FRAMEWORKS) $(LIBVLC) $(MODULES) $(LIBS) -install_name @loader_path/../Frameworks/VLC.framework/Version/Current/VLC -o VLC.framework/Version/Current/VLC && \
	install_name_tool -change /usr/local/lib/libvlc-control.0.dylib \
	                          `pwd`/VLC.framework/lib/libvlc-control.dylib \
	                   VLC.framework/Version/Current/VLC && \
	install_name_tool -change /usr/local/lib/libvlc.1.dylib \
	                          `pwd`/VLC.framework/lib/libvlc.dylib \
	                   VLC.framework/Version/Current/VLC && \
	touch VLC.framework

VLC.framework:: VLC.framework/Version/Current/VLC
 
.PHONY: clean

clean:
	rm -Rf VLC.framework
	rm -Rf $(OBJECTS) *.o $(SRC_DIR)/*.o
