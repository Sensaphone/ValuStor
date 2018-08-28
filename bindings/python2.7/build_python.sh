#!/bin/sh
swig -c++ -python valustor.i
g++ -Wall -Wextra -g -O2 -std=c++11 -fPIC -DPIC ValuStorWrapper.cpp valustor_wrap.cxx -o valustor.so -shared -lstdc++ -L/usr/local/lib -lcassandra -lpthread -I../.. -I/usr/include/python2.7 -I/usr/include/python2.7/config -lpython2.7 -lm -ldl -fpic -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
