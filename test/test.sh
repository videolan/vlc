#! /bin/sh

cd ..
export PYTHONPATH=$PYTHONPATH:bindings/python/build/lib.linux-i686-2.3:test/build/lib.linux-i686-2.3

python test/test.py -v
