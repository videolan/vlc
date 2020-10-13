find -name "vlc**.tar.xz" -print0 | xargs -0 -I {} mv {} .
rm -rf download*
ls | grep *.tar.xz | xargs tar -xvf
rm *.tar.xz
cd vlc*
sudo mkdir /home/voiceroy
./configure --prefix=/home/voiceroy/opt/
export CC=gcc-10
export CC=gcc-10
export CXX=g++-10
export LD=g++-10
make -j4
make dist-gzip
find -name "*.tar.gz" -print0 | xargs -0 -I {} mv {} ..
