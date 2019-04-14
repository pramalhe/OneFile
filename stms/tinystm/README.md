TinySTM
=======

OVERVIEW
--------

TinySTM is a lightweight but efficient word-based STM implementation.
This distribution includes three versions of TinySTM: write-back
(updates are buffered until commit time), write-through (updates are
directly written to memory), and commit-time locking (locks are only
acquired upon commit).  The version can be selected by editing the
makefile, which documents all the different compilation options.

TinySTM compiles and runs on 32 or 64-bit architectures.  It was tested
on various flavors of Unix, on Mac OS X, and on Windows using cygwin.
It comes with a few test applications, notably a linked list, a skip
list, and a red-black tree.


INSTALLATION
------------

TinySTM requires the 'atomic\_ops' library, freely available from
[www.hpl.hp.com](http://www.hpl.hp.com/research/linux/atomic_ops/).
A stripped-down version of the library is included in the TinySTM 
distribution.  If you wish to use another version, you must set the 
environment variable LIBAO\_HOME to the installation directory of
'atomic\_ops'.

If your system does not support GCC thread-local storage, modify the
TLS parameter in the 'Makefile' file.

To compile TinySTM libraries, execute 'make' in the main directory.  To
compile test applications, execute 'make test'.  To check the compiled
library, execute 'make check'. 'make clean' will remove all compiled
files.
To compile the TinySTM GCC compatible library, execute 'make abi-gcc'.
To compile test applications, execute 'make abi-gcc-test'.


CONTACT
-------

* E-mail : [tinystm@tinystm.org](mailto:tinystm@tinystm.org)
* Web    : [http://tinystm.org](http://tinystm.org) and
 [http://www.tmware.org](http://www.tmware.org)


ACKNOWLEDGEMENT
---------------

This library was supported by the European research consortium
[VELOX](http://www.velox-project.eu).
