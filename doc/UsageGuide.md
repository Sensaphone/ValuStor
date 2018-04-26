# Usage Guide

Topics
- [Memcached Replacement](#memcached-replacement)
- [JSON Document Storage](#json-document-storage)

## Memcached Replacement
This guide shows how to replace the C/C++ [libmemcached](http://docs.libmemcached.org/index.html) library with ValuStor.

### Configuration ###
```C++
// libmemcached configuration
const char* hosts= "--SERVER=host1 --SERVER=host12"
memcached_st *memc = memcached(hosts, strlen(hosts);
// ... memcached_pool_*() for server pools.
// ... memcached_set_encoding_key() for AES encryption.
// ... memcached_server_*() for server management
// ... memcached_flush_buffers() for connection management
memcached_free(memc);

// ValuStor configuration
ValuStor::ValuStor<std::string, std::string> store(
 {
  {"table", "database.table"},
  {"key_field", "key_field"},
  {"value_field", "value_field"},
  {"username", "username"},
  {"password", "password"},
  {"hosts", "host1,host2"},
  {"server_trusted_cert", "/etc/scylla/server.crt"},
 });
```
For ValuStor this configuration includes multi-threading built into the library.
Multi-threading with libmemcached must be programmed by the client software.


### Reads ###
Single reads are handled like this:
```C++
// libmemcached
std::string key = "key to find";
memcached_return rc;
size_t value_length = 0;
uint32_t flags = 0;
char* result = memcached_get(memc, key.c_str(), key.size(), &value_length, &flags, &rc);
if(rc == MEMCACHED_SUCCESS){
  std::string value_found(result);
}
else{
  std::string error_message(memcached_last_error_message(memc));
}

// ValuStor
auto result = store.retrieve("key to find");
if(result){
  std::string value_found = result.data;
}
else{
  std::string error_message = result.result_message;
}
```

The following shows how to read multiple values:
```C++
// libmemcached
const char* keys[] = {"key1", "key2", "key3"};
size_t key_count = sizeof(keys) / sizeof(const char*);
size_t key_length[key_count];
memcached_return rc = memcached_mget(memc, keys, key_length, key_count);
char *return_value;
char return_key[MEMCACHED_MAX_KEY];
size_t return_key_length;
size_t return_value_length;
uint32_t flags;
while((return_value= memcached_fetch(memc, return_key, &return_key_length, &return_value_length, &flags, &rc))){
  std::string key(return_key, return_key_length);
  std::string val(return_value, return_value_length);
  free(return_value);
  ...
}

// ValuStor
std::vector<std::string> keys = {"key1", "key2", "key3"};
for(auto& key : keys){
  auto result = store.retrieve(key);
  if(result){
    std::string val = result.data;
    ...
  }
}
```
Multiple simultaneous reads with ValuStor are normally not required: performance is very high just issuing multiple read requests.
The cassandra client driver will automatically make use of multi-threading and multiple server connections to maximize throughput.
If multiple reads are desired for application use, a compound key with a clutering key can be used.

The client driver will also automatically figure out which database node contains the key and direct the request there.
Memcached requires the additional use of the memcached_*get*_by_key() functions (not shown here) to perform this action.

### Writes ###
Single writes are handled like this:
```C++
// libmemcached
std::string key = "key to use";
std::string value = "value to write";
time_t expiration = time(nullptr) + 1000; // or '0' for no expiration
memcached_return rc = memcached_set(memc, key_to_use.c_str(), key_to_use.size(), value.c_str(), value.size(), expiration_time, 0);

// ValuStor
auto result = store.store("key to use", "value to write", 1000); // or '0' for no expiration
```

Like reads, there is no reason in ValuStor to batch multiple writes.
Just call `store()` multiple times and the client driver will automatically multi-thread the work as needed.
There is no need for functions like memcached_*set*_by_key() as the data is stored on the correct database nodes automatically.
Perform multiple writes as follows:
```C++
// ValuStor
std::vector<std::pair<std::string, std::string>> kvp = {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3}};
for(auto& pair : kvp){
  store.store(pair.first, pair.second);
}
```

To keep the interface simple, ValuStor does not (currently) implement a delete function.
Data can be deleted from ValuStor quickly by setting the expiration date to 1 second:
```C++
// ValuStor
auto result = store.retrieve("key to delete");
if(result){
  store.store("key to delete", result.data, 1); // will be deleted in 1 second
}
```

NOTE: ValuStor does not support [atomicity](/#atomicity) across multiple reads and/or writes, so there are no strict equivalents for
`memcached_replace()`, `memcached_add()`, `memcached_prepend()`, `memcached_append()` and `memcached_cas()`.

## JSON Document Storage
ValuStor can be used as a JSON document store.
Follow the [integration instructions](https://github.com/Sensaphone/ValuStor#json) to enable JSON for Modern C++ support in ValuStor.

It is important to setup an appropriate primary key.
This can be as simple as a UUID or a more complex combination of fields.
If a [clustering key](https://docs.datastax.com/en/cql/3.1/cql/ddl/ddl_compound_keys_c.html) is used for storing multiple documents
under the same partition key, the total size of the documents should be kept under 10MB to maximize cache efficiency.
Multiple documents can only be retrieved in a single `retrieve()` if they share the same partition key.

If you handle your JSON data in string form, you can do the following:
```C++
#include "nlohmann/json.hpp"
#include "ValuStor.hpp"
...
ValuStor::ValuStor<nlohmann::json, int64_t> docstore("example.conf");

std::string raw_json = "{\"key\":\"value\"}";
auto json = nlohmann::json::parse(raw_json);

int64_t document_id = 12345678;

// Store the document
docstore.store(document_id, json);

// Retrieve the document
auto result = docstore.retrieve(document_id);
if(result){
  raw_json = result.data.dump();
}
```

Rather than using strings to hold the JSON, you can just use the JSON library.

```C++
...
nlohmann::json document;

document["key1"] = "value";
document["key2"] = 1234;
document["key3"] = false;
document["key4"] = { 1, 2, 3, 4, 5 };

docstore.store(document_id, document);

```

The following example uses a string as the partition key and a UUID revision number as the clustering key.
```C++
#include "nlohmann/json.hpp"
#include "ValuStor.hpp"
...
// CREATE TABLE documents.docs (document_id text, revision uuid, json text, PRIMARY KEY (document_name, revision))
ValuStor::ValuStor<nlohmann::json, std::string, CassUuid> docstore("example.conf");

std::string document_id = "unique_document_id:1234";
CassUuidGen* uuid_gen = cass_uuid_gen_new();

// Create the document
nlohmann::json document;
document["name"] = "my document";
document["subject"] = "something important;
document["body"] = "secret information";
{
  CassUuid revision;
  cass_uuid_gen_time(uuid_gen, revision);
  docstore.store(document_id, revision, document);
}

// Change the contents of the document
document["body"] = "Super Secret Information";
{
  CassUuid revision;
  cass_uuid_gen_time(uuid_gen, revision);
  docstore.store(document_id, revision, document);
}

// Retrieve each document revision.
auto results = docstore.retrieve(document_id, CassUuid{}/*don't care*/, 1);
for(auto& result_pair : results.results){
  CassUuid revision = std::get<1>(result_pair.second);
  cass_uint8_t uuid_version = cass_uuid_version(revision); // version 1 date/time UUID
  cass_uint64_t timestamp = cass_uuid_timestamp(revision);
  nlohmann::json doc = result_pair.first;
  ...
}
```
