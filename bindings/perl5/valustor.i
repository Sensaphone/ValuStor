#ifdef SWIG
%module valustor
%{
#include "ValuStorWrapper.hpp"
%}
#endif

%include "std_string.i"

%include "ValuStorWrapper.hpp"
