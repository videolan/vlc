EXTRA_DIST = $(SOURCES_testvlc)
MOSTLYCLEANFILES = $(check_SCRIPTS)

noinst_SCRIPTS = testvlc.exe
dist_check_SCRIPTS = missing_api

SOURCES_testvlc = testvlc.cs
LDADD_testvlc = -lib:../src -r:VideoLAN.LibVLC.dll

testvlc.exe: $(SOURCES_testvlc)
	$(CSC) -target:exe -out:$@ $(CSFLAGS) $^ $(LDADD_testvlc)

TESTS = missing_api
