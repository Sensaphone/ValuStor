// ****************************************************************************************************
// ****************************************************************************************************
// ValuStorNativeWrapper
// ****************************************************************************************************
// ****************************************************************************************************
#include "ValuStor.hpp"
#include "ValuStorWrapper.hpp"

static std::unique_ptr<ValuStor::ValuStor<NATIVE_VAL_TYPE, NATIVE_KEY_TYPE>> native_database;

//
// Name:  default constructor
//
ValuStorNativeWrapper::ValuStorNativeWrapper(void){
  native_database.reset(new ValuStor::ValuStor<NATIVE_VAL_TYPE, NATIVE_KEY_TYPE>({
    {"table", "table.values"},
    {"key_field", "key"},
    {"value_field", "value"},
    {"hosts", "127.0.0.1"}
  }));
}

//
// Name:  destructor
//
ValuStorNativeWrapper::~ValuStorNativeWrapper(void){
  native_database.reset();
}

//
// Name:  instance
//
ValuStorNativeWrapper& ValuStorNativeWrapper::instance(void){
  static ValuStorNativeWrapper store;
  return store;
} // instance

//
// Name:  retrieve
//
NATIVE_VAL_TYPE ValuStorNativeWrapper::retrieve(NATIVE_KEY_TYPE key){
  ValuStorNativeWrapper::instance();
  auto result = native_database->retrieve(key);
  return result.data;
}

//
// Name:  store
//
bool ValuStorNativeWrapper::store(NATIVE_KEY_TYPE key, long value){
  ValuStorNativeWrapper::instance();
  auto result = native_database->store(key, value);
  return result.error_code == ValuStor::SUCCESS;
} // store

//
// Name:  close
//
void ValuStorNativeWrapper::close(void){
  native_database.reset();
}



// ****************************************************************************************************
// ****************************************************************************************************
// ValuStorWrapper
// ****************************************************************************************************
// ****************************************************************************************************

static std::unique_ptr<ValuStor::ValuStor<WRAPPED_VAL_TYPE, WRAPPED_KEY_TYPE>> string_database;

//
// Name:  default constructor
//
ValuStorWrapper::ValuStorWrapper(void){
  string_database.reset(new ValuStor::ValuStor<WRAPPED_VAL_TYPE, WRAPPED_KEY_TYPE>({
    {"table", "table.values"},
    {"key_field", "key"},
    {"value_field", "value"},
    {"hosts", "127.0.0.1"}
  }));
}

//
// Name:  destructor
//
ValuStorWrapper::~ValuStorWrapper(void){
  string_database.reset();
}

//
// Name:  instance
//
ValuStorWrapper& ValuStorWrapper::instance(void){
  static ValuStorWrapper store;
  return store;
} // instance

//
// Name:  retrieve
//
std::string ValuStorWrapper::retrieve(std::string key){
  ValuStorWrapper::instance();
  auto result = string_database->retrieve(std::get<0>( string_database->stringToKey(key) ));
  return string_database->valueToString(result.data);
}

//
// Name:  store
//
bool ValuStorWrapper::store(std::string key, std::string value){
  ValuStorWrapper::instance();
  auto result = string_database->store( std::get<0>( string_database->stringToKey(key) ), string_database->stringToValue(value));
  return result.error_code == ValuStor::SUCCESS;
} // store

//
// Name:  close
//
void ValuStorWrapper::close(void){
  string_database.reset();
}
