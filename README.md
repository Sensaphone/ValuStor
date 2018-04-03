PURPOSE
-------
This project is intended to replace a memcached key-value pair database with a superior alternative.
Memcached has a number of out-of-the-box limitations including lack of persistent storage,
type-inflexibility, and no direct redundancy or failover capabilities. To resolve these issues,
we can use the [ScyllaDB](https://www.scylladb.com), a Cassandra-compatible database written in C++.

ScyllaDB is a disk-backed data store with an in-RAM cache. As such, ScyllaDB performs extremely well
for this application. In most cases, the entire data set can be stored in the database cache, resulting
in 100% cache hits. Under the rare circumstances where data is not in the ScyllaDB cache, the database
itself is one of the highest performing disk-based databases that exists anywhere. A single properly
spec'ed server can serve as many as a million transactions per second even if it has to hit the disk.

ScyllaDB is an eventually-consistent database, which is perfect for a cache. Memcached makes no
guarantees that a key will return a value that was previously stored. When a memcached node goes down
that data is lost. ScyllaDB, on the other hand, will almost always return something, even if it is an
older version. Inconsistencies can be repaired and resolved easily and happen with much less frequency.

ScyllaDB also supports tunable consistency. This project makes use of this by seeking high levels of
consistency, but allowing for lower levels of consistency in exchange for availability. It is possible
to tune this to require full quorum-level consistency that mirrors memcached's all or nothing 
availability.

Because ScyllaDB is a fully typed database, we can do more than just "string => string" key-value pairs.
At the moment this project intends to support both integers and strings with more to come.

There is one important caveat. While memcached allows support for a fixed memory profile, the ScyllaDB
data store does not. To maintain absolute RAM-based access performance, enough memory is required to 
store the full data set. Alternatively, precision use of TTL records for automatic deletion of old cache
records is possible. Another option is just to allow some records to fall out of the ScyllaDB cache and
be reloaded from disk from time-to-time. The extreme performance of ScyllaDB makes this relatively
painless for most applications.

For ease-of-use, it is single-file, header-only C++11-compatible project.


CONFIGURATION
-------------
All configuration options, including server information, are the top of the header file.

The only requirement is to set the following:
`
  #define SCYLLA_DB_TABLE     std::string("<database>.<table>")
  #define SCYLLA_KEY_FIELD    std::string("<key field>")
  #define SCYLLA_VALUE_FIELD  std::string("<value field>")
  #define SCYLLA_USERNAME     std::string("<username>")
  #define SCYLLA_PASSWORD     std::string("<password>")
  #define SCYLLA_IP_ADDRESSES std::string("<ip_address_1>,<ip_address_2>,<ip_address_3>")`

Given a schema of a scylla table...
`
  CREATE TABLE cache.values (
    key bigint PRIMARY KEY,
    value text
  ) WITH compaction = {'class': 'SizeTieredCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'};`

...the following are used in the configuration:
`
  <database> = cache
  <table> = values
  <key field> = key
  <value field> = value`

API
---
There are only two public functions in the API:
`
  ValueStore::Result store(long key, std::string value, InsertMode_t insert_mode)
  ValueStore::Result retrieve(long key)`

Insert modes are ValueStore::DISALLOW_BACKLOG, ValueStore::ALLOW_BACKLOG, and ValueStore::USE_ONLY_BACKLOG.

The ValueStore::Result has the following data members:
`
  ErrorCode_t error_code
  std::string result_message
  std::string data`

The ValueStore::ErrorCode_t is one of the following:
`
  ValueStore::UNKNOWN_ERROR = -8
  ValueStore::BIND_ERROR = -7
  ValueStore::QUERY_ERROR = -6
  ValueStore::CONSISTENCY_ERROR = -5
  ValueStore::PREPARED_SELECT_FAILED = -4
  ValueStore::PREPARED_INSERT_FAILED = -3
  ValueStore::SESSION_FAILED = -2
  ValueStore::INVALID_KEY = -1
  ValueStore::SUCCESS = 0
  ValueStore::NOT_FOUND = 1`

USAGE
-----

Code:
`
  auto store_result = ValueStore::store(1234, "value");
  if(store_result){
    auto retrieve_result = ValueStore::retrieve(1234);
    if(retrieve_result){
      std::cout << 1234 << " => " << result.data << std::endl;
    }
    else{
      std::cerr << retrieve_result.result_message << std::endl;
    }
  }
  else{
    std::cerr << store_result.result_message << std::endl;
  }`

Output:
`
  1234 => value`


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

KNOWN ISSUES
-------------
1) If the client cannot connect to a server and never has, failed ValueStore::store() calls will use the backlog queue.
   Even if the server becomes accessible, the backlog thread will not begin to process.
   The back log thread will only start working after a successful ValueStore::store() call.

2) If the backlog receives two entries with the same key, it will not remove the older one.

3) The backlog appears to be poorer performing than not using it even though batching should theoretically be faster.

4) The code currently only supports "integer => string" key-value pairs.

5) Retrievals do not check the backlog.

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
