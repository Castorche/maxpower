#!/usr/bin/python

import os
import sys
import getpass
import subprocess

try:
	from fabricate import *
except ImportError, e:
	print "Couldn't find the fabricate module. Make sure you have sourced config.sh"
	sys.exit(1)


def get_maxpower_dir():
	dir = os.environ.get('MAXPOWERDIR')
	if dir == None:
		dir = os.getcwd() + "/../../../../.."
		print "MAXPOWERDIR undefined, using: %s" % (dir)
	return dir
	


MAXOSDIR = os.environ['MAXELEROSDIR']
MAXCOMPILERDIR = os.environ['MAXCOMPILERDIR']
MAXPOWERDIR=get_maxpower_dir()


MAXFILE = 'LMemCpuAccessTest.max'
DESIGN_NAME = MAXFILE.replace('.max', '')
sources = ['lmem_cpu_access_test.c']
target = 'cpu_access_test'
includes = ['-I%s/src/maxpower/lmem/cpu_access/runtime/' % (MAXPOWERDIR)] 
MAXPOWER_LIBS = ['-L%s/src/maxpower/lmem/runtime/' % (MAXPOWERDIR),	
				 '-L%s/src/maxpower/lmem/cpu_access/runtime/' % (MAXPOWERDIR), '-llmem_cpu_access', '-llmem' ]



def sliccompile():
	"""Compiles a maxfile in to a .o file"""
	run("%s/bin/sliccompile" % (MAXCOMPILERDIR), MAXFILE, MAXFILE.replace('.max', '.o'))


def get_maxcompiler_inc():
    """Return the includes to be used in the compilation."""
    return ['-I.', '-I%s/include' % MAXOSDIR, '-I%s/include/slic' % MAXCOMPILERDIR]

def get_maxcompiler_libs():
    """Return the libraries to be used in linking."""
    return ['-L%s/lib' % MAXCOMPILERDIR, '-L%s/lib' % MAXOSDIR, '-lslic', '-lmaxeleros', '-lm', '-lpthread']

def get_ld_libs():
    """Returns the libraries to be used for linking."""
    return MAXPOWER_LIBS + get_maxcompiler_libs() +  [MAXFILE.replace('.max', '.o')]


cflags = ['-ggdb', '-O2', '-fPIC', 
		  '-std=gnu99', '-Wall', '-Werror', 
		  '-DDESIGN_NAME=%s' % (DESIGN_NAME)] + includes + get_maxcompiler_inc() 

def build():
    compile()
    link()

def compile():
	sliccompile()
	for source in sources:
		run('gcc', cflags, '-c', source, '-o', source.replace('.c', '.o'))

def link():
    objects = [s.replace('.c', '.o') for s in sources]
    run('gcc', objects, get_ld_libs(), '-o', target)

def clean():
    autoclean()

def getSimName():
	return getpass.getuser() + 'Sim'


def maxcompilersim():
	return '%s/bin/maxcompilersim' % MAXCOMPILERDIR

def start_sim():
	subprocess.call([maxcompilersim(), '-n', getSimName(), '-c', 'ISCA', 'restart'])

def stop_sim():
	subprocess.call([maxcompilersim(), '-n', getSimName(), 'stop'])

def restart_sim():
	start_sim()	
	
def sim_debug():
	subprocess.call(['maxdebug', '-g', 'graph_%s' % getSimName(), '-d',  '%s0:%s' % (getSimName(), getSimName()), MAXFILE])


main()
