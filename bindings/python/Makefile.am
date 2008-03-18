##############################################################################
# Building the Python binding
###############################################################################

EXTRA_DIST = vlcglue.h setup.py vlcwrapper.py vlcwidget.py \
	vlc_module.c \
	vlc_mediacontrol.c vlc_position.c \
	vlc_instance.c vlc_input.c

if BUILD_PYTHON

all:
if HAVE_WIN32
	COMPILERARG=--compiler=mingw32
else
	COMPILERARG=
endif
	srcdir="`cd $(srcdir);pwd`" top_builddir="$(abs_top_builddir)" python "$(srcdir)/setup.py" build $(COMPILERARG)

install:
	top_builddir="$(abs_top_builddir)" srcdir="`cd $(srcdir);pwd`" python $(srcdir)/setup.py install

clean:
	$(RM) -rf build
endif
