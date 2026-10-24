"""Headless metadata check; does not activate the plugin or call host callbacks."""
import ctypes
import pathlib
import sys

for arg in sys.argv[1:]:
    dll = ctypes.CDLL(str(pathlib.Path(arg).resolve()))
    dll.PlugInName.restype = ctypes.c_char_p
    dll.About.restype = ctypes.c_char_p
    assert dll.PlugInName() == b'ActivityTracker'
    about = dll.About()
    assert b'1.4.0' in about
    assert b'https://github.com/pytlus93/aw-watcher-plsql-developer' in about
    print('PASS SDK identity and About metadata:', arg)
