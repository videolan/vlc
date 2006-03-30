##############################################################################
# Building the Python binding
###############################################################################

EXTRA_DIST = vlcglue.c vlcglue.h setup.py vlcwrapper.py

if BUILD_PYTHON

if HAVE_WIN32
	COMPILERARG = --compiler=mingw32
else
	COMPILERARG = 
endif

all:
	srcdir="$(srcdir)" top_builddir="$(top_builddir)" python "$(srcdir)/setup.py" build $(COMPILERARG) "--build-base=$(top_builddir)/bindings/mediacontrol-python" "--build-temp=$(top_builddir)/bindings/mediacontrol-python"

# FIXME: python setup.py install does not have any option to install from a different build directory
# so this will not work in a separate builddir
install:
	python $(srcdir)/setup.py install

clean:
	$(RM) -rf build
endif
