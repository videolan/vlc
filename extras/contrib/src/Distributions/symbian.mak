#Compiled but not linked libtool broken .faad .mpeg2 .ogg .dvbpsi
all: .a52 .mad  \
     .lame .ffmpeg
LDFLAGS=-L$(EPOCROOT)/../cls-gcc/arm-none-symbianelf/lib -L$(EPOCROOT)/epoc32/release/armv5/lib -nostdlib -shared -Wl,--no-undefined $(EPOCROOT)/epoc32/release/armv5/lib/libm.dso $(EPOCROOT)/epoc32/release/armv5/lib/libc.dso  $(EPOCROOT)/epoc32/release/armv5/lib/libz.dso
EXTRA_CPPFLAGS=-D_UNICODE -D__GCCE__ -D__SYMBIAN32__ -D__S60_3X__ -D__FreeBSD_cc_version -include $(SYMBIAN_INCLUDE)/gcce/gcce.h  -I$(SYMBIAN_INCLUDE)/stdapis -I$(SYMBIAN_INCLUDE)/stdapis/sys -I$(SYMBIAN_INCLUDE)/variant -I$(SYMBIAN_INCLUDE)
