#
# Borland C++ project
# we override everything by setting win32_CUSTOM
#
win32_CUSTOM = yes

../win32.so: Makefile
	rm -f win32.mak
	$(BCBUILDER)/Bin/bpr2mak win32.bpr -s | sed 's#^LIBPATH = .*#&;$$(RELEASELIBPATH)# ; s#^USERDEFINES = .*#& -DWIN32 -D__PLUGIN__ -D__VLC__ -DMODULE_NAME_IS_win32 -DMODULE_NAME=win32 -DMODULE_PATH=modules_gui_win32_win32#' > win32.mak
	$(BCBUILDER)/Bin/make -f win32.mak -b


