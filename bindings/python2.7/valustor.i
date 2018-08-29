#ifdef SWIG
%module valustor

%include "std_string.i"

%{
#define SWIG_FILE_WITH_INIT
#include "ValuStorWrapper.hpp"
%}
#endif

%include "std_string.i"

%include "ValuStorWrapper.hpp"
