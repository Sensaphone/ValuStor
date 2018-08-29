CC		    =	g++
INCLUDES	= -I../.. -I/usr/include/python2.7 -I/usr/include/python2.7/config
LIBS		  =	-lstdc++ -L/usr/local/lib -lcassandra -lpthread -lpython2.7 -lm -ldl
LIBFLAGS 	= $(LIBS)
CFLAGS 		=	-Wall -Wextra -g -O2 -std=c++11 $(INCLUDES) -fPIC -DPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

CPFILES  = ValuStorWrapper.cpp valustor_wrap.cxx
HFILES   = ValuStorWrapper.hpp
OBJFILES = valustor_wrap.o

SOURCEFILES = $(HFILES) $(CPFILES)

default: all

valustor_wrap.cxx: ValuStorWrapper.hpp
	swig -c++ -python valustor.i

valustor_wrap.o: Makefile ValuStorWrapper.hpp valustor_wrap.cxx
	$(CC) $(CFLAGS) -c valustor_wrap.cxx -o valustor_wrap.o

ValuStorWrapper.o: Makefile ValuStorWrapper.hpp ValuStorWrapper.cpp ../../ValuStor.hpp
	$(CC) $(CFLAGS) -c ValuStorWrapper.cpp -o ValuStorWrapper.o

_valustor.so: ValuStorWrapper.o valustor_wrap.o
	g++ $(CFLAGS) ValuStorWrapper.o valustor_wrap.o -shared -o _valustor.so $(LIBFLAGS)

#
# Must install "_valustor.so" into "/usr/lib/python2.7/distutils/"
#
# Example Script:
#  import valustor
#  ValuStorIntWrapper.retrieve(1234)
#
all: valustor_wrap.cxx valustor_wrap.o ValuStorWrapper.o _valustor.so
