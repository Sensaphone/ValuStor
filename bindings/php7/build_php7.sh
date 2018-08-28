#!/bin/sh
swig -c++ -php7 valustor.i
g++ -Wall -Wextra -g -O2 -std=c++11 -fPIC -DPIC ValuStorWrapper.cpp valustor_wrap.cxx -o valustor.so -shared -lstdc++ -L/usr/local/lib -lcassandra -lpthread -I../.. -I/usr/include/php/20151012 -I/usr/include/php/20151012/main -I/usr/include/php/20151012/Zend -I/usr/include/php/20151012/TSRM -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -fpic
# install valustor.so into "/usr/lib/php/20151012/" and enable dl() or put the .so in the php.ini file.
