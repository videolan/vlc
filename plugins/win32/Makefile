#
# Waveout module - the classic way to do things
#
waveout_SOURCES = waveout.c

#
# Borland C++ project
# we override everything by setting intfwin_CUSTOM
#
intfwin_CUSTOM = yes
BCBUILDER = "D:/Program Files/CBuilder5"

../intfwin.so: Makefile
	rm -f intfwin.mak
	$(BCBUILDER)/Bin/bpr2mak intfwin.bpr -s | sed 's#^LIBPATH = .*#&;$$(RELEASELIBPATH)# ; s#^USERDEFINES = .*#& -DWIN32 -DPLUGIN -DMODULE_NAME_IS_intfwin -DMODULE_NAME=intfwin#' > intfwin.mak
	$(BCBUILDER)/Bin/make -f intfwin.mak -b

