PURPOSE
-------
This project is a key-value pair database intended as an alternative to memcached.
It is an easy to use, single-file, header-only C++11-compatible project.

Memcached has a number of out-of-the-box limitations including lack of persistent storage,
type-inflexibility, and no direct redundancy or failover capabilities. To resolve these issues,
we can use the [ScyllaDB](https://www.scylladb.com), a Cassandra-compatible database written in C++.

ScyllaDB is a disk-backed data store with an in-RAM cache. As such, ScyllaDB performs extremely well
for this application. In many cases, the entire data set can be stored in the database cache, resulting
in 100% cache hits. Under the rare circumstances where data is not in the ScyllaDB cache, the database
itself is one of the highest performing disk-based databases that exists anywhere. A single properly
spec'ed server can serve as many as a million transactions per second even if it has to hit the disk.

ScyllaDB is an eventually-consistent database, which is perfect for many cache applications. Memcached 
makes no guarantees that a key will return a value that was previously stored. When a memcached node
goes down that data is lost. ScyllaDB, with built in redundancy, will almost always return something,
even if it is an older version. Inconsistencies can be repaired and resolved easily.

ScyllaDB lets you easily scale up as demand increases. With configurable levels of redundancy, you
can decide how many copies of each piece of data you want on each database node according to your
own tolerance for failure.

ScyllaDB also supports tunable consistency. This project makes use of this by seeking high levels of
consistency, but allowing for lower levels of consistency in exchange for availability. It is possible
to tune this to require full quorum-level consistency that mirrors memcached's all or nothing 
availability.

Because ScyllaDB is a fully typed database, we can do more than just "string => string" key-value pairs.
The project supports integers, floating-points, strings, bytes (blobs), and uuids.

There is one important caveat. While memcached allows support for a fixed memory profile, the ScyllaDB
data store does not. To maintain absolute RAM-based access performance, enough memory is required to 
store the full data set. Alternatively, precision use of TTL records for automatic deletion of old cache
records is supported. Another option is just to allow some records to fall out of the ScyllaDB cache and
be reloaded from disk from time-to-time. The extreme performance of ScyllaDB makes this relatively
painless for most applications.

KEY FEATURES
------------
* Single header-only implementation makes it easy to drop into C++ projects.
* Supports a variety of C++ data types in the keys and values.
 * 8-, 16-, 32-, and 64-bit signed integers
 * 32-bit unsigned integers
 * single- and double-precision floating point numbers
 * boolean
 * strings
 * binary byte arrays
 * UUID
* Simple API: Only a single store and a single retrieve function are needed. There is no need to write database queries.
* There is no need to batch read or write requests for performance.
* There is no special client configuration required for redundancy, scalability, or multi-thread performance.
* RAM-like performance for most applications

CONFIGURATION
-------------
All configuration options, including server information, are the top of the header file.

The only requirement is to set the following:
```C++
  #define SCYLLA_DB_TABLE     std::string("<database>.<table>")
  #define SCYLLA_KEY_FIELD    std::string("<key field>")
  #define SCYLLA_VALUE_FIELD  std::string("<value field>")
  #define SCYLLA_USERNAME     std::string("<username>")
  #define SCYLLA_PASSWORD     std::string("<password>")
  #define SCYLLA_IP_ADDRESSES std::string("<ip_address_1>,<ip_address_2>,<ip_address_3>")
```

Given a schema of a scylla table...
```sql
  CREATE TABLE cache.values (
    key bigint PRIMARY KEY,
    value text
  ) WITH compaction = {'class': 'SizeTieredCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'};
```

...the following are used in the configuration:
```
  <database> = cache
  <table> = values
  <key field> = key
  <value field> = value
```

The following types are supported in the 'key' or 'value' fields in the CREATE TABLE:
* int8 (int8_t)
* int16 (int16_t)
* int32 (int32_t)
* uint32 (uint32_t)
* int64 (int64_t)
* float (float)
* double (double)
* bool (cass_bool_t)
* string (std::string and char*)
* bytes (cass_byte_t*)
* uuid (CassUuid)

We don't support the following, but the c++ driver does:
* custom
* inet
* decimal
* collection
* tuple
* user-defined type

API
---
ValuStor is implemented as a template class using only a default constructor.
```C++
  template<typename Key_T, typename Val_T> class ValuStor
```

The public API is very simple:
```C++
  ValuStor::Result store(Key_T key, Val_T value, uint32_t seconds_ttl, InsertMode_t insert_mode)
  ValuStor::Result retrieve(Key_T key)
```

The optional seconds TTL is the number of seconds before the stored value expires in the database.

The optional insert modes are `ValuStor::DISALLOW_BACKLOG`, `ValuStor::ALLOW_BACKLOG`, and `ValuStor::USE_ONLY_BACKLOG`.

The ValuStor::Result has the following data members:
```C++
  ErrorCode_t error_code
  std::string result_message
  std::string data
```

The ValuStor::ErrorCode_t is one of the following:
```C++
  ValuStor::VALUE_ERROR
  ValuStor::UNKNOWN_ERROR
  ValuStor::BIND_ERROR
  ValuStor::QUERY_ERROR
  ValuStor::CONSISTENCY_ERROR
  ValuStor::PREPARED_SELECT_FAILED
  ValuStor::PREPARED_INSERT_FAILED
  ValuStor::SESSION_FAILED
  ValuStor::SUCCESS
  ValuStor::NOT_FOUND
```

USAGE
-----

Code:
```C++
  ValuStor<long, std::string> valuestore;
  auto store_result = valuestore.store(1234, "value");
  if(store_result){
    auto retrieve_result = valuestore.retrieve(1234);
    if(retrieve_result){
      std::cout << 1234 << " => " << result.data << std::endl;
    }
  }
```

Output:
```
  1234 => value
```


DEPENDENCIES
------------
The Cassandra C/C++ driver is required. See https://github.com/datastax/cpp-driver/releases
This project has only been tested with version 2.7.1, but in principle it should work with other versions.
If using g++, it must be linked with -L/path/to/libcassandra.so/ -lcassandra.

An installation of either Cassandra or ScyllaDB is required. The latter is strongly
recommended for this application, as the former has much worse performance. ScyllaDB is incredibly easy
to setup. This project has been tested with ScyllaDB v.2.x.

THREAD SAFETY
-------------
The cassandra driver fully supports multi-threaded access.
This project is completely thread safe.
It is lockless, except for the backlog. Locks are only held if needed and for as short a time as possible.
Multi-threaded inserts are generally higher performing than single-threaded inserts if it can use multiple CPU cores.

NOTE: The multi-threaded performance of the cassandra driver is higher performing than the backlog thread.
      Backlog should only be used to increase data availability, not to increase performance.

ATOMICITY
---------
All write operations are performed atomically, but depending on the consistency level unexpected results may occur.
If the order is strictly important, all reads and writes must be performed at QUORUM consistency or higher.
There is no way to read-and-modify (including prepending/appending) data atomically.

KNOWN ISSUES
-------------
The backlog has a number of known issues:
1. If the client cannot connect to a server and never has, failed ValuStor::store() calls will use the backlog queue.
   Even if the server becomes accessible, the backlog thread will not begin to process.
   The back log thread will only start working after a successful ValuStor::store() call.
1. If the backlog receives two entries with the same key, it will not remove the older one.
1. Backlog entries may be inserted out-of-order in some cases.
1. Retrievals do not check the backlog.

LICENSE
-------
MIT License

Copyright (c) 2017-2018 Sensaphone

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright holders
shall not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization.
