#! /bin/sh


cd ..
export PYTHONPATH=$PYTHONPATH:bindings/mediacontrol-python/build/lib.linux-i686-2.3:test/build/lib.linux-i686-2.3:test/build/lib.linux-x86_64-2.3

LD_LIBRARY_PATH=src/.libs/ python test/test.py -v
