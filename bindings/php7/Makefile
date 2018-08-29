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

valustor_wrap.cxx:
	swig -c++ -php7 valustor.i

valustor_wrap.o: Makefile ValuStorWrapper.hpp valustor_wrap.cxx
	$(CC) $(CFLAGS) -c valustor_wrap.cxx -o valustor_wrap.o

ValuStorWrapper.o: Makefile ValuStorWrapper.hpp ValuStorWrapper.cpp
	$(CC) $(CFLAGS) -c ValuStorWrapper.cpp -o ValuStorWrapper.o

valustor.so: ValuStorWrapper.o valustor_wrap.o
	g++ $(CFLAGS) ValuStorWrapper.o valustor_wrap.o -shared -o valustor.so $(LIBFLAGS)

# Choices:
#  1) Add "extension=/path/to/valustor.so" to php.ini
#  2) Install valustor.so into "/usr/lib/php/20151012"
#
# Example Script:
#  <?php
#    require('valustor.php');
#    print ValuStorIntWrapper::retrieve(1234);
#  ?>
#

all: valustor_wrap.cxx valustor_wrap.o ValuStorWrapper.o valustor.so
