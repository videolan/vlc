dvd_SOURCES = dvd.c dvd_access.c dvd_demux.c dvd_seek.c dvd_es.c dvd_ifo.c dvd_udf.c dvd_summary.c $(SRC_DVD_EXTRA)

EXTRA_DEP = ../../lib/libdvdcss.a ../../lib/libdvdcss.so

$(EXTRA_DEP): libdvdcss

libdvdcss:
	@cd ../../ && $(MAKE) libdvdcss
