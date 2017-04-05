import glob
from distutils.core import setup, Extension

discodb_module = Extension('discodb._discodb',
                           sources=['discodbmodule.c'],
                           libraries=['discodb'])

setup(name='discodb',
      version='0.5',
      description='An efficient, immutable, persistent mapping object.',
      author='Nokia Research Center',
      ext_modules=[discodb_module],
      packages=['discodb'])
