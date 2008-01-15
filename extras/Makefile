BUILDDIR ?= ..

zsh: zsh.cpp
	g++ ${CXXFLAGS} -I${BUILDDIR} -I${BUILDDIR}/include -I../include zsh.cpp ${BUILDDIR}/src/.libs/libvlc.so -o zsh `top_builddir=${BUILDDIR} ${BUILDDIR}/vlc-config vlc builtin --cflags --libs`
