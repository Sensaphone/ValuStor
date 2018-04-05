#include "ValuStor.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main(void){
  #if 1
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
