#include "ValuStor.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main(void){
  { ValuStor<std::string, std::string> store; }
  { ValuStor<int8_t, int8_t> store; }
  { ValuStor<int16_t, int16_t> store; }
  { ValuStor<int32_t, int32_t> store; }
  { ValuStor<uint32_t, uint32_t> store; }
  { ValuStor<int64_t, int64_t> store; }
  { ValuStor<float, float> store; }
  { ValuStor<double, double> store; }
  { ValuStor<bool, bool> store; }
  { ValuStor<CassUuid, CassUuid> store; }
  { ValuStor<cass_byte_t*, cass_byte_t*> store; }
  { ValuStor<char*, char*> store; }

  ValuStor<long, std::string> store;
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
