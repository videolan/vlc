dvdread_SOURCES = dvdread.c input_dvdread.c $(SRC_DVDREAD_EXTRA)

ifeq (1,$(NEED_LIBDVDCSS))
LIBDVDCSS_OBJ = ../../lib/libdvdcss.a ../../lib/libdvdcss.so
endif
LIBDVDREAD_OBJ = ../../lib/libdvdread.a ../../lib/libdvdread.so

EXTRA_DEP = $(LIBDVDCSS_OBJ) $(LIBDVDREAD_OBJ)

$(LIBDVDCSS_OBJ): libdvdcss

libdvdcss:
	@cd ../../ && $(MAKE) libdvdcss

$(LIBDVDREAD_OBJ): libdvdread

libdvdread:
	@cd ../../ && $(MAKE) libdvdread
