
from distutils.core import setup,Extension

setup(
    name='Seawolf',
    version='0.1dev',
    author='Chris Thunes',
    author_email='cmthunes@ncsu.edu',
    description='libseawolf bindings',

    package_dir={'seawolf': '.'},
    packages=['seawolf'],

    py_modules=['seawolf.seawolf',
                'seawolf.__init__',
                'seawolf.var',
                'seawolf.notify',
                'seawolf.logging'],

    ext_modules=[Extension(
            'seawolf._seawolf', ['seawolf.i'],
            swig_opts=['-I../../include/'],
            libraries=['seawolf'],
            include_dirs=['../../include/'],
            library_dirs=['../'],
            define_macros=[('_POSIX_C_SOURCE', '200112L'), ('_XOPEN_SOURCE', '600')]
            )],
      )
