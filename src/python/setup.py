
from distutils.core import setup,Extension

setup(name='Seawolf',
      version='0.1dev',
      description='libseawolf bindings',
      author='Chris Thunes',
      author_email='cmthunes@ncsu.edu',
      package_dir={'seawolf': '.'},
      packages=['seawolf'],
      ext_modules=[Extension('seawolf._seawolf', ['seawolf.i'],
                             swig_opts=['-I../../include/'],
                             libraries=['seawolf'],
                             include_dirs=['../../include/'],
                             library_dirs=['../'])],
      py_modules=['seawolf.seawolf', 'seawolf.__init__'],
      )
