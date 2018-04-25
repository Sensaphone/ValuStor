# Usage Guide

## JSON Document Storage
ValuStor can be used as a JSON document store.
Follow the JSON for Modern C++ [integration instructions](https://github.com/Sensaphone/ValuStor#json) to enable JSON support in ValuStor.

It is important to setup an appropriate primary key.
This can be as simple as a UUID or a more complex combination of fields.
If a [clustering key](https://docs.datastax.com/en/cql/3.1/cql/ddl/ddl_compound_keys_c.html) is used 
in order to store multiple documents under the same partition key, the total size of the documents should be kept under 10MB for
maximum storage efficiency. Multiple documents can only be retrieved in a single `retrieve()` if they share the same partition key.

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
