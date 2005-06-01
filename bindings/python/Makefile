ifdef HOMEDRIVE
	COMPILERARG = --compiler=mingw32
else
	COMPILERARG = 
endif

all:
	python setup.py build $(COMPILERARG)

install:
	python setup.py install

clean:
	$(RM) -rf build

