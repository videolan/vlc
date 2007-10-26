EXTRA_DIST = $(SOURCES_testvlc)
MOSTLYCLEANFILES = $(check_SCRIPTS)

check_SCRIPTS = testvlc.exe

SOURCES_testvlc = testvlc.cs
LDADD_testvlc = -lib:../src -r:VideoLAN.LibVLC.dll

testvlc.exe: $(SOURCES_testvlc)
	$(CSC) -target:exe -out:$@ $(CSFLAGS) $^ $(LDADD_testvlc)

