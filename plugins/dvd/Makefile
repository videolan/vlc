dvd_SOURCES = dvd.c input_dvd.c dvd_ifo.c dvd_udf.c dvd_summary.c $(SRC_DVD_EXTRA)

EXTRA_DEP = ../../lib/libdvdcss.a ../../lib/libdvdcss.so

$(EXTRA_DEP): libdvdcss

libdvdcss:
	@cd ../../ && $(MAKE) libdvdcss
