CC		    =	g++
INCLUDES	= -I../.. -I/usr/include/php/20151012 -I/usr/include/php/20151012/main -I/usr/include/php/20151012/Zend -I/usr/include/php/20151012/TSRM
LIBS		  =	-lstdc++ -L/usr/local/lib -lcassandra -lpthread
LIBFLAGS 	= $(LIBS)
CFLAGS 		=	-Wall -Wextra -g -O2 -std=c++11 $(INCLUDES) -fPIC -DPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

CPFILES  = ValuStorWrapper.cpp valustor_wrap.cxx
HFILES   = ValuStorWrapper.hpp
OBJFILES = valustor_wrap.o

SOURCEFILES = $(HFILES) $(CPFILES)

default: all

valustor_wrap.cxx: ValuStorWrapper.hpp
	swig -c++ -perl5 valustor.i

valustor_wrap.o: Makefile ValuStorWrapper.hpp valustor_wrap.cxx
	$(CC) -D_REENTRANT -D_GNU_SOURCE -DDEBIAN -fwrapv -fno-strict-aliasing -pipe -I/usr/local/include -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -O2 -g -fPIC -I/usr/lib/x86_64-linux-gnu/perl/5.22/CORE -c valustor_wrap.cxx -o valustor_wrap.o

ValuStorWrapper.o: Makefile ValuStorWrapper.hpp ValuStorWrapper.cpp ../../ValuStor.hpp
	$(CC) $(CFLAGS) -c ValuStorWrapper.cpp -o ValuStorWrapper.o

valustor.so: ValuStorWrapper.o valustor_wrap.o
	g++ $(CFLAGS) -fstack-protector-strong ValuStorWrapper.o valustor_wrap.o -shared -o valustor.so $(LIBFLAGS)

all: valustor_wrap.cxx valustor_wrap.o ValuStorWrapper.o valustor.so
