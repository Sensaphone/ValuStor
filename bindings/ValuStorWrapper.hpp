// ****************************************************************************************************
// ****************************************************************************************************
// ValuStorWrapper
// ****************************************************************************************************
// ****************************************************************************************************

#include <string>

//
// You must specifcy the underlying type used in the database.
// It will automatically be converted to/from a string in ValuStorWrapper.
// This allows any type to be used as a string in the target language.
// Valid types:
//   - bool
//   - char (int8_t)
//   - short (int16_t)
//   - int (int32_t)
//   - long (int64_t)
//   - float
//   - double
//   - std::string
//   - CassUuid
//   - nlohmann::json
// You may need to include the appropriate header file in the .cpp file to support these types.
//
#define WRAPPED_KEY_TYPE  std::string
#define WRAPPED_VAL_TYPE  std::string

// ****************************************************************************************************
/// @class         ValuStorWrapper
///
/// @brief         A wrapper class used for porting to other languages.
///                This wraps any valid underlying data type into a string in the target language.
///
class ValuStorWrapper
{
  private:
    ValuStorWrapper(void);
    ~ValuStorWrapper(void);
    static ValuStorWrapper& instance();

  public:
    // ****************************************************************************************************
    /// @name            retrieve
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           key
    ///
    /// @return          The string value associated with the key, or '' if not found.
    ///
    static std::string retrieve(std::string key);

    // ****************************************************************************************************
    /// @name            store
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           key
    /// @param           value
    ///
    /// @return          The result of the store.
    ///
    static bool store(std::string key, std::string value);

    // ****************************************************************************************************
    /// @name            close
    ///
    /// @brief           Gracefully close the database connection.
    ///
    static void close(void);
};


// ****************************************************************************************************
// ****************************************************************************************************
// ValuStorNativeWrapper
// ****************************************************************************************************
// ****************************************************************************************************

//
// Valid types:
//   - bool
//   - char (int8_t)
//   - short (int16_t)
//   - int (int32_t)
//   - long (int64_t)
//   - float
//   - double
//   - std::string
//   - CassUuid
//   - nlohmann::json
// You may need to include the appropriate header file in the .hpp file to support some types.
//
#define NATIVE_KEY_TYPE  long
#define NATIVE_VAL_TYPE  long

// ****************************************************************************************************
/// @class         ValuStorNativeWrapper
///
/// @brief         A wrapper class used for porting to other languages.
///                This wraps the underlying native data type directly into the target language.
///
class ValuStorNativeWrapper
{
  private:
    ValuStorNativeWrapper(void);
    ~ValuStorNativeWrapper(void);
    static ValuStorNativeWrapper& instance();

  public:
    // ****************************************************************************************************
    /// @name            retrieve
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           key
    ///
    /// @return          The string value associated with the key, or '' if not found.
    ///
    static NATIVE_VAL_TYPE retrieve(NATIVE_KEY_TYPE key);

    // ****************************************************************************************************
    /// @name            store
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           key
    /// @param           value
    ///
    /// @return          The result of the store.
    ///
    static bool store(NATIVE_KEY_TYPE key, NATIVE_VAL_TYPE value);

    // ****************************************************************************************************
    /// @name            close
    ///
    /// @brief           Gracefully close the database connection.
    ///
    static void close(void);
};
