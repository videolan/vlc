EXTRA_DIST = $(SOURCES_testvlc)
MOSTLYCLEANFILES = $(check_SCRIPTS)

noinst_SCRIPTS = testvlc.exe
dist_check_SCRIPTS = missing_api

SOURCES_testvlc = testvlc.cs
LDADD_testvlc = -lib:../src -r:VideoLAN.LibVLC.dll

testvlc.exe: $(SOURCES_testvlc) ../src/VideoLAN.LibVLC.dll
	$(CSC) -target:exe -out:$@ $(CSFLAGS) $(SOURCES_testvlc) \
		$(LDADD_testvlc)

TESTS = missing_api
