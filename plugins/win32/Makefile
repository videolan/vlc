#
# Waveout module - the classic way to do things
#
waveout_SOURCES = waveout.c

#
# Borland C++ project
# we override everything by setting intfwin_CUSTOM
#
intfwin_CUSTOM = yes

../intfwin.so: Makefile
	rm -f intfwin.mak
	$(BCBUILDER)/Bin/bpr2mak intfwin.bpr -s | sed 's#^LIBPATH = .*#&;$$(RELEASELIBPATH)# ; s#^USERDEFINES = .*#& -DWIN32 -D__PLUGIN__ -D__VLC__ -DMODULE_NAME_IS_intfwin -DMODULE_NAME=intfwin#' > intfwin.mak
	$(BCBUILDER)/Bin/make -f intfwin.mak -b

