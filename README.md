# ValuStor

## Summary
ValuStor is a key-value pair database solution originally designed as an alternative to memcached. It resolves a number of out-of-the-box 
limitations including lack of persistent storage, type-inflexibility, no direct redundancy or failover capabilities, poor scalability, and 
lack of TLS support. It can also be used for [JSON](#json) document storage and asynchronous distributed message queue applications. It is 
an easy to use, single-file, header-only C++11-compatible project.

This project wraps abstracted client-side key-value-pair database operations around the Cassandra client driver using a simple API.
It utilizes a [ScyllaDB](https://www.scylladb.com) database backend.

## Key Features
- Single header-only implementation makes it easy to drop into C++ projects.
- A optional [backlog](#backlog) queues data in the event that the database is temporarily inaccessible.
- [Adaptive](#consistencies) fault tolerance, consistency, and availability.
- [TLS support](#tls), including client authentication
- Supports a variety of native C++ data types in the keys and values.
  - 8-, 16-, 32-, and 64-bit signed integers
  - single- and double-precision floating point numbers
  - booleans
  - strings
  - binary data (blobs)
  - UUID
  - [JSON](#json)
- [Simple API](#api): Only a single `store()` and a single `retrieve()` function are needed. There is no need to write database queries.
- RAM-like database performance for most applications.
- There is no need to batch read or write requests for performance.
- There is no special client configuration required for redundancy, scalability, or multi-thread performance.

## Rationale
The key features require the use of a disk-backed data store. In order to compete with RAM-only database applications, such as memcached, 
it must also have good in-RAM caching. We can give the database server enough RAM to store its entire data set in cache, typically 
resulting in 100% cache hits. But what do we do if the data can only be found on disk? ScyllaDB is one of the highest performing, lowest 
latency, disk-based databases anywhere. It allows many cache misses while still maintaining the high level of performance required in many 
applications.

Using a NoSQL eventually-consistent database is advantageous for many cache applications. Memcached makes no guarantees that a key will 
return a value that was previously stored. When a memcached node goes down that data is lost. A ScyllaDB cluster, with built-in redundancy, 
can almost always return something. This project harnesses client-side [tunable consistency](#consistencies). It seeks high levels of 
consistency, but adaptively allows for lower levels of consistency in exchange for higher availability. Optional full quorum-level 
consistency can be used to mirror other databases' all or nothing availability.

With memcached you were limited to the amount of RAM allocated on each memcached node. There was no automatic way to scale ever higher 
because cache evictions increased cache misses. ScyllaDB lets you easily scale up arbitrarily as demand increases. With configurable levels 
of redundancy, you can decide how many copies of each piece of data you want on each database node according to your own tolerance for 
failure. We combine this with a [write backlog queue](#backlog) to further increase failure resistence.

By using a fully typed database, we can do more than just "string => string" key-value pairs. The project 
[supports](#database-setup) integers, floating-points, strings, bytes (blobs), UUIDs, and JSON. C++ templates make 
it easy to integrate different combinations, including compound key support.

There is one important caveat. While memcached allows support for a fixed memory profile, the underlying data store does not. Memcached 
keeps performance guarantees by evicting cached data, while ScyllaDB retrieves it from disk. The extreme database performance makes this 
negligible for many applications. However, to maintain strict absolute RAM-based access performance, [enough memory is 
required](http://docs.scylladb.com/faq/#do-i-ever-need-to-disable-the-scylla-cache-to-use-less-memory) to store the full data set. 
Alternatively, precision use of TTL records for automatic deletion of old cache records is supported.

## Backlog
This project incorporates a backlog to queue changes locally for times when the remote server is unavailable.
If a `store()` request fails for any reason, it can be cached in the backlog to be committed later.
In memcached this data would be lost or require the application to wait until the server returned.
This is well-suited to asynchronous producer/consumer applications where the producer doesn't want to wait around
for the server to become available and it is okay if the consumer gets the data eventually.
It's another layer of redundancy on top of an already solid database backend.

Backlog use is optional and its use can be selected individually for each `store()` request.

In order to maximize performance of the store functionality by using a (nearly) lockless design,
a few design trade-offs were made:
1. The backlog won't remove the older of multiple entries with the same key. This slightly reduces backlog performance.
2. `retrieve()` does not check the backlog.
3. If the client cannot connect to a server and never has, failed `store()` calls will use the backlog queue.
   Even if the server becomes accessible, the backlog thread will not begin to process automatically.
   The backlog processing will only being once the first `store()` call is successful.

## Consistencies
In many traditional synchronized database clusters any writes are guaranteed to be available by a quorum of nodes 
for any subsequent reads. However, if the number of available database nodes falls below quorum, no read or write 
operations can take place because they would not be consistent. It also makes managing multi-datacenter and 
multi-rack installations very brittle. For high-availability applications, this is unacceptable.

Because ScyllaDB is eventually-consistent, it does not suffer from any of these limitations. Consistency levels can 
be chosen dynamically by the client for both `store()` and `retrieve()` operations. An `ALL` or `QUORUM` level of 
consistency can be chosen to ensure the maximum level of consistency. The real power of tunable consistency comes in two
scenarios:
1. Operation below quorum
2. Performance

When the database is running below quorum, the read and write requests will fail. ValuStor can automatically lower 
the required level of consistency and try again. Eventually, as long as at least one redundant database node is up 
somewhere, the request will succeed. For many types of high-availability applications this is essential.

If absolute performance is required, both reads and writes can always operate below quorum-level consistency.
This increases the risk that a write won't be applied by the time a read occurs, but it will eventually become available.

The default consistencies used by ValuStor are `LOCAL_QUORUM, LOCAL_ONE, ONE` for `retrieve()` operations and 
`LOCAL_ONE, ONE, ANY` for `store()` operations. Requiring quorum on reads makes quorum on writes mostly 
unnecessary. `LOCAL_QUORUM` or `QUORUM` can be added to writes at the expense of client write performance. Add 
`QUORUM` to allow remote datacenters to be checked in the order given. The number of retry attempts at each 
consistency level can also be controlled using this approach (i.e. `QUORUM, QUORUM, ONE, ONE`).

## Configuration

### Dependencies
This project requires a C++11 compatible compiler. This project has been tested with g++ 5.4.0.

The Cassandra C/C++ driver is required. See https://github.com/datastax/cpp-driver/releases
This project has only been tested with version 2.7.1 and 2.8.1, but in principle it should work with other versions.
Example installation:
```sh
# Prerequisites: e.g. apt-get install build-essential cmake automake libtool libssl-dev

wget https://github.com/libuv/libuv/archive/v1.20.0.tar.gz
tar xvfz v1.20.0.tar.gz
cd libuv-1.20.0/
./autogen.sh
./configure
make
make install

wget https://github.com/datastax/cpp-driver/archive/2.8.1.tar.gz
tar xvfz 2.8.1.tar.gz
cd cpp-driver-2.8.1
mkdir build
cd build
cmake ..
make
make install
```
If using g++, `cassandra.h` must be in the include path and the application must be linked with 
`-L/path/to/libcassandra.so/ -lcassandra -lpthread`.

An installation of either Cassandra or ScyllaDB is required. The latter is strongly
recommended for this application due to its [advantageous design decisions](http://opensourceforu.com/2018/04/seven-design-decisions-that-apache-cassandras-successor-is-built-on/).
ScyllaDB is incredibly [easy to setup](http://docs.scylladb.com/getting-started/). This project has been tested with ScyllaDB v.2.x.
```sh
# Prerequisite: Install ScyllaDB

vi /etc/scylla/scylla.yaml
scylla_io_setup
service scylla-server start
```

### TLS
Using TLS for encryption and authentication is highly recommended. It is not difficult to setup. See the [instructions](doc/TLS.md).

### Database Setup
Configuration can use either a configuration file or setting the same configuration at runtime.
See the [API documentation](#api).
The only requirement is to set the following fields:
```
  table = <database>.<table>
  key_field = <key field>
  value_field = <value field>
  username = <username>
  password = <password>
  hosts = <ip_address_1>,<ip_address_2>,<ip_address_3>
```

The schema of a scylla table should be setup as follows:
```
  CREATE TABLE <database>.<table> (
    <key_field> bigint PRIMARY KEY,
    <value_field> text
  ) WITH compaction = {'class': 'SizeTieredCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'};
```
The following Cassandra data types (along with their C++ equivalent) are supported in the CREATE TABLE:
* tinyint (int8_t)
* smallint (int16_t)
* int (int32_t)
* bigint (int64_t)
* float (float)
* double (double)
* boolean (bool)
* varchar, text, and ascii (std::string and nlohmann::json)
* blob (std::vector<uint8_t>)
* uuid (CassUuid)

## API
ValuStor is implemented as a template class using two constructors.
See the [usage documentation](#usage).
```C++
  template<typename Val_T, typename Key_T...> class ValuStor

  ValuStor::ValuStor(std::string config_file)
  ValuStor::ValuStor(std::map<std::string, std::string> configuration_kvp)
```

The public API is very simple:
```C++
  ValuStor::Result store(Key_T... keys, Val_T value, uint32_t seconds_ttl, InsertMode_t insert_mode, int64_t microseconds_since_epoch)
  ValuStor::Result retrieve(Key_T... keys, size_t key_count)
```

Both single and compound keys are supported.

The optional seconds TTL is the number of seconds before the stored value expires in the database.
Setting a value of 0 means the record will not expire.
Setting a value of 1 is effectively a delete operation (after 1 second elapses).

The optional insert modes are `ValuStor::DISALLOW_BACKLOG`, `ValuStor::ALLOW_BACKLOG`, and `ValuStor::USE_ONLY_BACKLOG`.
If the backlog is disabled, any failures will be permanent and there will be no further retries.
If the backlog is enabled, failures will retry automatically until they are successful or the ValuStor object is deleted.

The optional microseconds since epoch can be specified to explicitly control which inserted records are considered to be current in the
database. It is possible for rapidly inserted stores *with the same key* to get applied out-of-order. Specifying this explicity removes all
ambiguity, but makes it especially important that `store()` calls from multiple clients use the same synchronized time source. The default
value of 0 lets the database apply the timestamp automatically. If no timestamp is explicitly given, stores added to the backlog will use
the timestamp of when they were added to the queue.

The optional key count is the number of keys to include in the WHERE clause of a value SELECT.
A key count of 0 (the default) means all keys are used and at most only one record can be returned.
If fewer than all the keys are used, the retrieval may return multiple records.
While all keys must be specified as function parameters, if you are only using a subset of keys, the values of the unused keys are "don't care".
NOTE: You must always specify a [partition key](https://docs.datastax.com/en/cql/3.1/cql/ddl/ddl_compound_keys_c.html) completely,
but you can leave out all or part of the clustering key.

The ValuStor::Result has the following data members:
```C++
  ErrorCode_t error_code
  std::string result_message
  Val_T data
  std::vector<std::pair<Val_T, std::tuple<Keys...>>> results;
```
Data for a single record (the default for `store()`) will be returned in `Result::data`.
Data for multiple records are returned in the `Result::results` along with the keys associated with each record.

Requests that fail to commit changes to the database store will return an unsuccessful error code,
unless the backlog mode is set to `USE_ONLY_BACKLOG`.
If the backlog mode is set to `ALLOW_BACKLOG`, then the change will eventually be committed.

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

## Usage

Writing code to use ValuStor is very easy.
You only need a constructor and then call the `store()` and `retrieve()` functions in any combination.
Connection management is automatic.

The following example shows the most basic key-value pair usage:

Code:
```C++
  #include "ValuStor.hpp"
  ...  

  // e.g. CREATE TABLE cache.values (key_field bigint, value_field text, PRIMARY KEY (key_field))
  //
  //                    <value>    <key>
  ValuStor::ValuStor<std::string, int64_t> store("example.conf");
  auto store_result = store.store(1234, "value");
  if(store_result){
    auto retrieve_result = store.retrieve(1234);
    if(retrieve_result){
      std::cout << 1234 << " => " << result.data << std::endl;
    }
  }
```
  
Output:
```  
  1234 => value
```

You can use a file to load the configuration (as above) or
specify the configuration in your code (as below).
See the [example config](example.conf) for more information.

The following example uses a compound key and a multi-select retrieval.

Code:
```C++
  #include "ValuStor.hpp"
  ...  
  
  // e.g. CREATE TABLE cache.values (k1 bigint, k2 bigint, v text, PRIMARY KEY (k1, k2))
  ValuStor::ValuStor<std::string, int64_t, int64_t> store({
        {"table", "cache.values"},
        {"key_field", "k1, k2"},
        {"value_field", "v"},
        {"username", "username"},
        {"password", "password"},
        {"hosts", "127.0.0.1"}
  });

  store.store(1234, 10, "first");
  store.store(1234, 20, "last");
  auto retrieve_result = store.retrieve(1234, -1/*don't care*/, 1);
  for(auto& pair : retrieve_result.results){
    std::cout << "{\"k1\":" << std::get<0>(pair.second)
              << ", \"k2\":" << std::get<1>(pair.second)
              << ", \"v\":\"" << pair.first << "\""
              << std::endl;
  }
```

Output:
```
  {"k1":1234, "k2":10, "v":"first"}
  {"k1":1234, "k2":20, "v":"last"}
```

## JSON
This package integrates with [JSON for Modern C++](https://github.com/nlohmann/json) for easy document storage.
The values will be serialized and deserialized automatically.
The serialization uses strings and thus requires a cassandra `text` or `varchar` field.
To use it, include the json header before the ValuStor header:
```C++
#include "nlohmann/json.hpp"
#include "ValuStor.hpp"
...
ValuStor::ValuStor<nlohmann::json, int64_t> valuestore("example.conf");
```

## Thread Safety
The cassandra driver fully supports multi-threaded access.
This project is completely thread safe.
It is lockless, except for using the backlog. Locks are only held if needed and for as short a time as possible.
Higher performance can be achieved by utilizing multiple threads and cores to make concurrent `store()` calls.

NOTE: The multi-threaded performance of the cassandra driver is higher performing than the backlog thread.
      The backlog should only be used to increase data availability, not to increase performance.
      It is single-thread and uses locking, so it will always have worse performance.

## Atomicity
All write operations are performed atomically, but depending on the consistency level unexpected results may occur.
If the order is strictly important, all reads and writes must be performed at QUORUM consistency or higher.
There is no way to read-and-modify (including prepending/appending) data atomically.

## Future
There are a number of new features on the roadmap.
1. Usage guide for asynchronous distributed message queue applications.
2. File storage and access, à la [GridFS/Mongofiles](https://docs.mongodb.com/manual/reference/program/mongofiles/)
3. Command line programs useful for scripting, etc.
4. Counter type support (increment/decrement).

## History
This project was created at [Sensaphone](https://www.sensaphone.com) to solve memcached's lack of redundancy, 
insufficient scalability, and poor performance. After the decision was made to replace it,
it only took 3 days from inception to production. The transition was flawless. Due to our success with it
and subsequent interest from other developers in the community, it was developed as
[free and open-source software](https://en.wikipedia.org/wiki/Free_and_open-source_software).

## License
MIT License

Copyright (c) 2017-2018 [Sensaphone](https://www.sensaphone.com)

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
