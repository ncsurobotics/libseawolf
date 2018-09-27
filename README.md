NOTE: Building documentation does not work out of the box. See (this comment)[https://github.com/ncsurobotics/libseawolf/issues/4#issuecomment-424905719] on issue #4 for a fix.


Prerequisites
-------------

To build libseawolf should require only a modern POSIX compliant system and GNU
Make. Additional tools are required to build the documentation (see "Building
Documentation" below).



Building
--------

Under Linux to build and install under /usr/local,

  make && make install

Under FreeBSD or OpenBSD with gmake installed,

  gmake && gmake install

To install to a different prefix set the PREFIX environment variable when
running make install, e.g.,

  PREFIX=/usr make install



Cross Compiling
---------------

Support for building the library component and hub for Blackfin systems running
uCLinux is included in this standard distribution. The Blackfin toolchain must
be present in the system path. To build run,

  CONFIG=build/config.bfin.mk make install

The library and includes will be installed into a top level folder called
'bfin'. This can be changed in the same way as the prefix for standard builds.

To crosscompile for other platforms the Blackfin configuration can be used as a
starting point. libseawolf should be able to build on any platform with a
complete POSIX environment.



Building Documentation
----------------------

To build the HTML documentation for libseawolf requires the Doxygen,
imagemagick, and graphviz packages. If these packages are installed then the
documentation can be built by issuing,

  make doc

from the top level directory. If this process completes without error (a few
warnings are to be expected) then the documentation can be found in the
doc/html/ directory. If you are unable to build the documentation then a recent
copy should be available online at http://docs.ncusrobotics.com.



Python Bindings
---------------

Python bindings can be built for libseawolf allowing you to use libseawolf from
your Python applications. Building the bindings requires SWIG and development
headers for the version of Python you plan to use. On Debian/Ubuntu systems
these are available as the swig and python-dev respectively. With these
dependencies installed the bindings can be built by executing

  make pylib

If this succeeds then the module can be installed with

  sudo make pylib-install

This will build and install the module using the default python executable. This
can be changed by setting the PYTHON variable, e.g.

  sudo make pylib-install PYTHON=/usr/bin/python2.5

For instructions on using the Python bindings, see the online documentation.
