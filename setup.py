#!/usr/bin/env python
# Tamito KAJIYAMA <13 September 2001>
# $Id: setup.py,v 1.3 2001/09/22 23:17:00 kajiyama Exp $

from distutils.core import setup, Extension

setup (name = "ebmodule",
       version = "2.1",
       description = "A wrapper module of the EB library",
       author = "Tamito KAJIYAMA",
       author_email = "kajiyama@grad.sccs.chukyo-u.ac.jp",
       url = "http://pseudo.grad.sccs.chukyo-u.ac.jp/~kajiyama/python/",
       py_modules = [
           "eblib"],
       ext_modules = [
           Extension("ebmodule", ["src/ebmodule.c"],
                     include_dirs=["/usr/local/include","./src"],
                     #library_dirs=["/home/plateau/src/ebmodule-2.0/src/eb/.libs","/home/plateau/src/ebmodule-2.0/src/eb_lib/zlib/.libs"],
                     libraries=["eb", "z"])])
