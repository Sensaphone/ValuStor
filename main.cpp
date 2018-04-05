#include "ValuStor.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main(void){
  { ValuStor<std::string, std::string> store("example.conf"); }
  { ValuStor<int8_t, int8_t> store("example.conf"); }
  { ValuStor<int16_t, int16_t> store("example.conf"); }
  { ValuStor<int32_t, int32_t> store("example.conf"); }
  { ValuStor<uint32_t, uint32_t> store("example.conf"); }
  { ValuStor<int64_t, int64_t> store("example.conf"); }
  { ValuStor<float, float> store("example.conf"); }
  { ValuStor<double, double> store("example.conf"); }
  { ValuStor<bool, bool> store("example.conf"); }
  { ValuStor<CassUuid, CassUuid> store("example.conf"); }
  { ValuStor<cass_byte_t*, cass_byte_t*> store("example.conf"); }
  { ValuStor<char*, char*> store("example.conf"); }

  #if 0
  ValuStor<long, std::string> store({
        {"table", "cache.values"},
        {"key_field", "key_field"},
        {"value_field", "value_field"},
        {"username", "username"},
        {"password", "password"},
        {"ip_addresses", "127.0.0.1"}
  });
  #else
  ValuStor<long, std::string> store("example.conf");
  #endif

  {
    auto result = store.store(1234, "something", 60);
    if(result){
      {
        auto result = store.retrieve(1234);
        if(result){
          std::cout << "Success: " << result.result_message << std::endl;
        }
        else{
          std::cout << "Failed to retrieve: " << result.result_message << std::endl;
        } // else
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 61));

      {
        auto result = store.retrieve(1234);
        if(not result){
          std::cout << "Success: " << result.result_message << std::endl;
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
