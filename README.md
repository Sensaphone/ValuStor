# ValuStor
ValuStor is a key-value pair database solution originally designed as an alternative to memcached. It resolves a 
number of out-of-the-box limitations including lack of persistent storage, type-inflexibility, no direct redundancy 
or failover capabilities, poor scalability, and lack of SSL support. It can also be used for [JSON](#json) document 
storage and asynchronous distributed message queue applications. It is an easy to use, single-file, header-only C++11-compatible 
project.

This project wraps key-value pair operations around [ScyllaDB](https://www.scylladb.com), a Cassandra-compatible 
database written in C++. It is a disk-backed data store with an in-RAM cache. In many cases, the entire data set 
can be stored in the database cache, resulting in 100% cache hits. If the data can only be found on disk, ScyllaDB 
is still one of the highest performing, lowest latency, disk-based databases anywhere.

ScyllaDB is a NoSQL eventually-consistent database, which is advantageous for many cache applications. Memcached 
makes no guarantees that a key will return a value that was previously stored. When a memcached node goes down that 
data is lost. A ScyllaDB cluster, with built in redundancy, can almost always return something. This project makes 
use of [tunable consistency](#consistencies) by seeking high levels of consistency, but adaptively allowing for 
lower levels of consistency in exchange for higher availability. Full quorum-level consistency can be used to 
mirror other databases' all or nothing availability.

With memcached you were limited to the amount of RAM allocated on each memcached node. There was no automatic way 
to scale ever higher because cache evictions increased cache misses. ScyllaDB lets you easily scale up arbitrarily 
as demand increases. With configurable levels of redundancy, you can decide how many copies of each piece of data 
you want on each database node according to your own tolerance for failure.

By using a fully typed database, we can do more than just "string => string" key-value pairs. The project 
[supports](#configuration) integers, floating-points, strings, bytes (blobs), UUIDs, and JSON. C++ templates make 
it easy to integrate different combinations.

There is one important caveat. While memcached allows support for a fixed memory profile, the ScyllaDB data store 
does not. Memcached keeps performance guarantees by evicting cached data, while ScyllaDB retrieves it from disk. 
The extreme performance of ScyllaDB makes this negligible for many applications. However, to maintain strict 
absolute RAM-based access performance, [enough memory is 
required](http://docs.scylladb.com/faq/#do-i-ever-need-to-disable-the-scylla-cache-to-use-less-memory) to store the 
full data set. Alternatively, precision use of TTL records for automatic deletion of old cache records is 
supported.

## Key Features
* Single header-only implementation makes it easy to drop into C++ projects.
* A optional [backlog](#backlog) queues data in the event that the database is temporarily inaccessible.
* [Adaptive](#consistencies) fault tolerance, consistency, and availability.
* SSL support, including client authentication
* Supports a variety of native C++ data types in the keys and values.
 * 8-, 16-, 32-, and 64-bit signed integers
 * single- and double-precision floating point numbers
 * boolean
 * strings
 * binary data (blobs)
 * UUID
 * [JSON](#json)
* [Simple API](#api): Only a single `store()` and a single `retrieve()` function are needed. There is no need to write database queries.
* RAM-like performance for most applications.
* There is no need to batch read or write requests for performance.
* There is no special client configuration required for redundancy, scalability, or multi-thread performance.

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
1. If the backlog receives two entries with the same key, it will not remove the older one.
   They will be applied in chronological order, however, so eventually the newer one will replace the older.
1. Older backlog entries may be processed after newer successful non-backlogged `store()` requests.
   The default backlog mode should not be used if losing backlogged data is more acceptable than the chance of 
   having overwritten newer data.
   You can configure `store()` requests to only use the backlog.
   While this reduces maximum performance, it eliminates any data consistency issues.
1. `retrieve()` does not check the backlog.
1. If the client cannot connect to a server and never has, failed `store()` calls will use the backlog queue.
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
Configuration can use either a configuration file or setting the same configuration at runtime.
See the [API documentation](#api).
The only requirement is to set the following fields:
```
  table = <database>.<table>
  key_field = <key field>
  value_field = <value field>
  username = <username>
  password = <password>
  ip_addresses = <ip_address_1>,<ip_address_2>,<ip_address_3>
```

The schema of a scylla table should be setup as follows:
```cql
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
  template<typename Key_T, typename Val_T> class ValuStor

  ValuStor::ValuStor(std::string config_file)
  ValuStor::ValuStor(std::map<std::string, std::string> configuration_kvp)
```

The public API is very simple:
```C++
  ValuStor::Result store(Key_T key, Val_T value, uint32_t seconds_ttl, InsertMode_t insert_mode)
  ValuStor::Result retrieve(Key_T key)
```

The optional seconds TTL is the number of seconds before the stored value expires in the database.
Setting a value of 0 means the record will not expire.
Setting a value of 1 is effectively a delete operation (after 1 second elapses).

The optional insert modes are `ValuStor::DISALLOW_BACKLOG`, `ValuStor::ALLOW_BACKLOG`, and `ValuStor::USE_ONLY_BACKLOG`.
If the backlog is disabled, any failures will be permanent and there will be no further retries.
If the backlog is enabled, failures will retry automatically until they are successful or the ValuStor object is deleted.

The ValuStor::Result has the following data members:
```C++
  ErrorCode_t error_code
  std::string result_message
  Val_T data
  size_t size
```
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

Code:
```C++
  #include "ValuStor.hpp"
  ...  

  ValuStor::ValuStor<int64_t, std::string> store("example.conf");
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

Code:
```C++
  #include "ValuStor.hpp"
  ...  
  
  ValuStor::ValuStor<int64_t, std::string> store({
        {"table", "cache.values"},
        {"key_field", "key_field"},
        {"value_field", "value_field"},
        {"username", "username"},
        {"password", "password"},
        {"ip_addresses", "127.0.0.1"}
  });
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

## JSON
This package integrates with [JSON for Modern C++](https://github.com/nlohmann/json) for easy document storage.
The values will be serialized and deserialized automatically.
The serialization uses strings and thus requires a cassandra `text` or `varchar` field.
To use it, include the json header before the ValuStor header:
```C++
#include "nlohmann/json.hpp"
#include "ValuStor.hpp"
...
ValuStor::ValuStor<int64_t, nlohmann::json> valuestore("example.conf");
```

## Dependencies
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
1. Compound key support (with multiple reads)
2. Improved support for asynchronous distributed message queue applications.
3. File storage and access, à la [GridFS/Mongofiles](https://docs.mongodb.com/manual/reference/program/mongofiles/)
4. Command line programs useful for scripting, etc.
5. Counter type support (increment/decrement)

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
