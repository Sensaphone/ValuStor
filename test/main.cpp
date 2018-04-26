#include "ValuStor.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main(void){
  //
  // CREATE TABLE cache.tbl111 (k tinyint PRIMARY KEY, v blob) WITH compaction = {'class': 'SizeTieredCompactionStrategy'} AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'};
  //
  ValuStor::ValuStor<std::vector<uint8_t>, int8_t> store(
   {
    {"table", "cache.tbl111"},
    {"key_field", "k"},
    {"value_field", "v"},
    {"username", ""},
    {"password", ""},
    {"hosts", "sensadb1.sensaphone.net"},
    {"server_trusted_cert", "/etc/scylla/keys/scylla.crt, /etc/scylla/keys/client.crt"},
    {"server_verify_mode", "3"},
    {"client_log_level", "5"},
    {"client_ssl_cert", "/etc/scylla/keys/client.crt"},
    {"client_ssl_key", "/etc/scylla/keys/client.key"}
  });
  {
    std::vector<uint8_t> arr{ 0x01, 0x02, 0x03, 0x04, 0x08, 0x10, 0x20 };
    auto result = store.store(12, arr, 2);
    if(result){
      {
        auto result = store.retrieve(12);
        if(result){
          std::cout << "Success: " << result.result_message << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 3));
	      {
        	auto result = store.retrieve(12);
	        if(not result){
        	  std::cout << "Success: " << result.result_message << std::endl;
	        }
        	else{
	          std::cout << "Failed to retrieve: " << result.result_message << std::endl;
        	} // else
	      }
        }
        else{
          std::cout << "Failed to retrieve: " << result.result_message << std::endl;
        } // else
      }
    }
    else{
      std::cout << "Failed to store: " << result.result_message << std::endl;
    } // else
  }
  return 0;
}
