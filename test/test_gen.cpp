#include <vector>
#include <map>
#include <iostream>
using namespace std;

int main(void){
  std::vector<std::string> key_types = {
    "tinyint",
    "smallint",
    "int",
    "bigint",
    "float",
    "double",
    "text",
    "varchar",
    "ascii",
    "uuid"
  };

  std::vector<std::string> value_types = {
    "tinyint",
    "smallint",
    "int",
    "bigint",
    "float",
    "double",
    "boolean",
    "text",
    "varchar",
    "ascii",
    "uuid"
  };

  int x = 1;
  #if 0
  //
  // This is for generating cql.txt
  //
  cout << "CREATE KEYSPACE cache WITH REPLICATION = {'class':'SimpleStrategy','replication_factor':1};" << endl;
  for(std::string key : key_types){
    for(std::string val : value_types){
      std::cout << "CREATE TABLE cache.tbl" << std::to_string(x++) << " (k "
	   << key << " PRIMARY KEY, v " << val << ") WITH compaction = {'class': 'SizeTieredCompactionStrategy'}"
	   << " AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'};"
	   << std::endl;
    } // for
  } // for
  #endif

  cout << "#include \"ValuStor.hpp\"" << endl;
  cout << "#include <iostream>" << endl;
  cout << "int main(void){" << endl;
  cout << "  CassUuidGen* uuid_gen = cass_uuid_gen_new();" << endl;
  cout << "  CassUuid uuid1;" << endl;
  cout << "  cass_uuid_gen_random(uuid_gen, &uuid1);" << endl;
  cout << "  CassUuid uuid2;" << endl;
  cout << "  cass_uuid_gen_random(uuid_gen, &uuid2);" << endl;
  cout << "  CassUuid uuid3;" << endl;
  cout << "  cass_uuid_gen_random(uuid_gen, &uuid3);" << endl;

  std::map<std::string, std::vector<std::string>> vals_for_testing = {
    {"int8_t", {"0", "-1", "1"}},
    {"int16_t", {"0", "-1", "1"}},
    {"int32_t", {"0", "-1", "1"}},
    {"int64_t", {"0", "-1", "1"}},
    {"float", {"0.0", "-1.0", "1.0"}},
    {"double", {"0.0", "-1.0", "1.0"}},
    {"bool", {"true", "false", "true"}},
    {"std::string", {"\"asdf\"", "\"nada\"", "\"QWERTY\""}},
    {"CassUuid", {"uuid1", "uuid2", "uuid3"}}
  };

  x = 1;
  for(std::string key : key_types){
    key = key == "tinyint" ? "int8_t" :
          key == "smallint" ? "int16_t" :
          key == "int" ? "int32_t" :
          key == "bigint" ? "int64_t" :
          key == "float" ? "float" :
          key == "double" ? "double" :
	        key == "boolean" ? "bool" :
          key == "varchar" ? "std::string" :
          key == "text" ? "std::string" :
          key == "ascii" ? "std::string" :
          key == "uuid" ? "CassUuid" :
                          "";
    for(std::string val : value_types){
      val = val == "tinyint" ? "int8_t" :
            val == "smallint" ? "int16_t" :
            val == "int" ? "int32_t" :
            val == "bigint" ? "int64_t" :
            val == "float" ? "float" :
            val == "double" ? "double" :
            val == "boolean" ? "bool" :
            val == "varchar" ? "std::string" :
            val == "text" ? "std::string" :
            val == "ascii" ? "std::string" :
            val == "uuid" ? "CassUuid" :
                            "";

      cout << "{" << endl
           << "  ValuStor::ValuStor<" << val << ", " << key << "> store({"
           << "{\"table\", \"cache.tbl" << std::to_string(x++) << "\"},"
           << "{\"key_field\", \"k\"},"
           << "{\"value_field\", \"v\"},"
           << "{\"username\", \"\"},"
           << "{\"password\", \"\"},"
           << "{\"hosts\", \"127.0.0.1\"},"
           << "{\"server_trusted_cert\", \"/etc/scylla/keys/scylla.crt\"}"
           << "});";

      int ndx = 0;
      for(auto& k : vals_for_testing.at(key)){
        auto v = vals_for_testing.at(val).at(ndx++);

        cout << endl
             << "  {auto result = store.store(" << k << ", " << v << ", 0);"
             << " if(result){ auto result2 = store.retrieve(" << k << "); "
             << "if(not result2){ std::cout << \"READ ERROR\" << std::endl; }"
             << "} else { std::cout << \"WRITE ERROR: "
             << "tbl" << std::to_string(x-1) << " (" << key << ") => " << "(" << val << ")\" << std::endl; }}";
      } // for

      cout << endl << "}" 
           << endl;
    } // for
  } // for
  cout << "}" << endl;

  return 0;
}


