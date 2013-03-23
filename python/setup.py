import glob, sys
from distutils.core import setup, Extension

if sys.platform == 'darwin':
    extra_compile_args = ['-fnested-functions']
else:
    extra_compile_args = []

discodb_module = Extension('discodb._discodb',
                           sources=['discodbmodule.c'] + glob.glob('../src/*.c'),
                           include_dirs=['../src'],
                           libraries=['cmph'],
                           extra_compile_args=extra_compile_args)

setup(name='discodb',
      version='0.2',
      description='An efficient, immutable, persistent mapping object.',
      author='Nokia Research Center',
      ext_modules=[discodb_module],
      packages=['discodb'])
