#include "ValuStor.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main(void){
  ValuStor::ValuStor<int64_t, std::vector<uint8_t>> store({{"table", "cache.tbl113"},{"key_field", "k"},{"value_field", "v"},{"username", ""},{"password", ""},{"ip_addresses", "127.0.0.1"}});
  {
    std::vector<uint8_t> arr{ 0x01, 0x02, 0x03, 0x04, 0x08, 0x10, 0x20 };
    auto result = store.store(123457, arr);
    if(result){
      {
        auto result = store.retrieve(1234);
        if(result){
          std::cout << "Success: " << result.result_message << std::endl;
          //std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 3));
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
