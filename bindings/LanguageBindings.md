# ValuStor Language Bindings

Language bindings for ValuStor can be created using [SWIG](http://www.swig.org).
Download and install the latest version.
Edit the ValuStorWrapper.hpp and ValuStorWrapper.cpp to customize the language binding.
Each directory contains a Makefile that can then be used to build the bindings.

Additional instructions are given below.

## PHP7
To use the PHP7 extension, you must do one of the following:
- Install the valustor.so into the PHP modules directory (e.g. /usr/lib/php/20151012)
- Add "extension=/path/to/valustor.so" to php.ini
- Enable dl() in the php.ini file (not recommended)

Once the extension has been installed, you must include 'valustor.php' into your repository
and use it like this:
```php
<?php
  require('valustor.php');
  print ValuStorIntWrapper::retrieve(1234);
?>
```

## Python
To use the Python module, install "_valustor.so" into the python distutils directory.
(e.g. "/usr/lib/python2.7/distutils/").

Once the extension has been installed, you must include 'valustor.py' into your repository
and use it like this:
```python
from valustor import *
ValuStorIntWrapper.retrieve(1234)
```

## Perl5
To use the Perl module, include valustor.so and valustor.pm in your repository.

Once the extension has been installed, you can run it like this:
```perl
use valustor;
print valustor::ValuStorIntWrapper::retrieve(1234);
```
