#!/usr/bin/python

import os
import sys

try:
from fabricate import *
except ImportError, e:
	print "Couldn't find the fabricate module. Make sure you have sourced config.sh"
	sys.exit(1)

MAXOSDIR = os.environ['MAXELEROSDIR']
MAXCOMPILERDIR = os.environ['MAXCOMPILERDIR']

sources = ['maxhash.c', 'maxhash_slic.c']
target_dir = 'lib/'
target = 'libmaxhash-slic.a'
includes = [ '-Iinclude' ] 

SYSTEMLIBS = ['']

def get_maxcompiler_inc():
    """Return the includes to be used in the compilation."""
    return ['-I.', '-I%s/include' % MAXOSDIR, '-I%s/include/slic' % MAXCOMPILERDIR]

cflags = ['-ggdb', '-O2', '-fPIC', '-std=gnu99', '-Wall', '-Werror'] + includes + get_maxcompiler_inc() 

def build():
    compile()
    lib()

def compile():
    for source in sources:
        run('gcc', cflags, '-c', source, '-o', source.replace('.c', '.o'))

def lib():
    objects = [s.replace('.c', '.o') for s in sources]
    run('ar', '-cq', target, objects)

def clean():
    autoclean()

main()
