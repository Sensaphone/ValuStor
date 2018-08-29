// ****************************************************************************************************
/// @class         ValuStorIntWrapper
///
/// @brief         A wrapper class used for porting to other languages.
///
class ValuStorIntWrapper
{
  private:
    ValuStorIntWrapper(void);
    ~ValuStorIntWrapper(void);
    static ValuStorIntWrapper& instance();

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
    static long retrieve(long key);

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
    static bool store(long key, long value);
};
