#include "ValuStor.hpp"
#include "ValuStorWrapper.hpp"

static std::unique_ptr<ValuStor::ValuStor<int64_t, int64_t>> int_database;

//
// Name:  default constructor
//
ValuStorIntWrapper::ValuStorIntWrapper(void){
  int_database.reset(new ValuStor::ValuStor<int64_t, int64_t>({
    {"table", "table.values"},
    {"key_field", "key"},
    {"value_field", "value"},
    {"hosts", "127.0.0.1"}
  }));
}

//
// Name:  destructor
//
ValuStorIntWrapper::~ValuStorIntWrapper(void){}

//
// Name:  instance
//
ValuStorIntWrapper& ValuStorIntWrapper::instance(void){
  static ValuStorIntWrapper store;
  return store;
} // instance

//
// Name:  retrieve
//
long ValuStorIntWrapper::retrieve(long key){
  ValuStorIntWrapper::instance();
  auto result = int_database->retrieve(key);
  return result.data;
}

//
// Name:  store
//
bool ValuStorIntWrapper::store(long key, long value){
  ValuStorIntWrapper::instance();
  auto result = int_database->store(key, value);
  return result.error_code == ValuStor::SUCCESS;
} // store
