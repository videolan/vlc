##############################################################################
# Building the Python binding
###############################################################################

# FIXME
#ifdef HOMEDRIVE
#	COMPILERARG = --compiler=mingw32
#else
#	COMPILERARG = 
#endif

if BUILD_PYTHON

all:
	python setup.py build $(COMPILERARG)

install:
	python setup.py install

clean:
	$(RM) -rf build
endif
