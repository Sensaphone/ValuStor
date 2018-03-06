#include "ValueStore.hpp"

#include <iostream>
#include <chrono>

int main(void){
  auto result = ValueStore::retrieve(1234);
  std::cout << result.result_message << std::endl;
  if(result){
    std::cout << "Success" << std::endl;
  }
  else{
    std::cout << "Failed" << std::endl;
  } // else
  return 0;
}
