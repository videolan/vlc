tar -xvzf vlc*.tar.xz
rm *.tar.xz
mv vlc* vlc
./configure --prefix=$HOME/opt/
export CC=gcc-10
export CPP=g++-10
export CXX=g++-10
export LD=g++-10
make -j4 
make dist-gzip
