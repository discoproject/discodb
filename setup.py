import glob
from setuptools import find_packages, setup
from distutils.core import Extension

discodb_module = Extension('discodb._discodb',
                           sources=['python/discodbmodule.c'] + glob.glob('src/*.c'),
                           include_dirs=['src'],
                           libraries=['cmph'])

setup(name='discodb',
      version='0.2',
      description='An efficient, immutable, persistent mapping object.',
      author='Nokia Research Center',
      ext_modules=[discodb_module],
      packages=find_packages('python'),
      package_dir={'': 'python'})
