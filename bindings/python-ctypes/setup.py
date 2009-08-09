from distutils.core import setup
import sys
import os
import generate

vlc_include_path = os.path.join("..","..","include","vlc")
if not os.path.exists(vlc_include_path):
    raise Exception("This script should be run from a VLC tree.")

files = [ os.path.join(vlc_include_path, filename)
          for filename in os.listdir(vlc_include_path) ]

generate.process('vlc.py', files)

setup(name='python-vlc',
      version = '1.1.0',
      author='Olivier Aubert',
      author_email='olivier.aubert@liris.cnrs.fr',
      url='http://wiki.videolan.org/PythonBinding',
      py_modules=['vlc'],
      keywords = [ 'vlc', 'video' ],
      license = "GPL",
      description = "VLC bindings for python.",
      long_description = """VLC bindings for python.

This module provides ctypes-based bindings for the native libvlc API
(see http://wiki.videolan.org/ExternalAPI) and the MediaControl API
(see http://wiki.videolan.org/PythonBinding) of the VLC video player.

It is automatically generated from the include files.
"""
      )
