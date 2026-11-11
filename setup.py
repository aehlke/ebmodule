#!/usr/bin/env python
# Tamito KAJIYAMA <13 September 2001>
# $Id: setup.py,v 1.3 2001/09/22 23:17:00 kajiyama Exp $

from setuptools import setup, Extension
#from ez_setup import use_setuptools
#use_setuptools()
import os
import sys

#from setuptools import setup
#from setuptools.extension import Extension

IS_WINDOWS = sys.platform in ('win32', )

if IS_WINDOWS:
    EBMODULE_C = r'src\ebmodule.c'
    INCLUDE_DIRS = [r'C:\EBLibrary\include', r'.\src']
    LIBRARY_DIRS = [r'C:\EBLibrary\lib']
else:
    EBMODULE_C = 'src/ebmodule.c'
    INCLUDE_DIRS = ['/usr/local/include', './src']
    LIBRARY_DIRS = None

# Pin headers and the library to the same owned prefix during qualification.
EB_LIBRARY_PREFIX = os.environ.get('EB_LIBRARY_PREFIX')
if EB_LIBRARY_PREFIX:
    EB_LIBRARY_PREFIX = os.path.abspath(EB_LIBRARY_PREFIX)
    INCLUDE_DIRS = [os.path.join(EB_LIBRARY_PREFIX, 'include'), './src']
    LIBRARY_DIRS = [os.path.join(EB_LIBRARY_PREFIX, 'lib')]

setup (name = 'ebmodule',
       version = '2.3',
       python_requires = '>=3.10',
       description = 'A wrapper module of the EB library',
       author = 'Tamito KAJIYAMA',
       author_email = 'kajiyama@grad.sccs.chukyo-u.ac.jp',
       url = 'http://pseudo.grad.sccs.chukyo-u.ac.jp/~kajiyama/python/',
       py_modules = ['eblib'],
       ext_modules = [
           Extension('eb', [EBMODULE_C],
                     include_dirs=INCLUDE_DIRS,
                     library_dirs=LIBRARY_DIRS,
                     #library_dirs=['/home/plateau/src/ebmodule-2.0/src/eb/.libs','/home/plateau/src/ebmodule-2.0/src/eb_lib/zlib/.libs'],
                     libraries=['eb'])]) #, 'z'])])
