CS = gmcs
CSFLAGS =

TARGETS = VideoLAN.VLC.Control.dll testvlc.exe

all: $(TARGETS)

clean:
	rm -f -- $(TARGETS) *.netmodule

VideoLAN.VLC.Control.dll: marshal.cs libvlc.cs exception.cs
testvlc.exe: testvlc.cs VideoLAN.VLC.Control.dll

%.netmodule: %.cs Makefile
	$(CS) -target:module -out:$@ $(CSFLAGS) $(filter %.cs,$^) \
		$(patsubst %,-addmodule:%,$(filter %.netmodule,$^)) \

%.dll: Makefile
	$(CS) -target:library -out:$@ $(CSFLAGS) $(filter %.cs,$^) \
		$(patsubst %,-addmodule:%,$(filter %.netmodule,$^)) \
		$(patsubst %,-r:%,$(filter %.dll,$^))

%.exe: Makefile
	$(CS) -target:exe -out:$@ $(CSFLAGS) $(filter %.cs,$^) \
		$(patsubst %,-addmodule:%,$(filter %.netmodule,$^)) \
		$(patsubst %,-r:%,$(filter %.dll,$^))

