/*
MIT License 

Copyright (c) 2017-2018 Sensaphone

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright holders
shall not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization.
*/

#ifndef VALUE_STORE_H
#define VALUE_STORE_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

// See https://github.com/datastax/cpp-driver/releases
// This has been tested with version 2.7.1.
#include <cassandra.h>

// ****************************************************************************************************
/// @class         ValuStor
///
/// @brief         A base object class from which all top level classes may inherit.
///                An object implements enough basic methods to support rudimentary object reflection.
///
template<typename Key_T, typename Val_T>
class ValuStor
{
  public:
    typedef enum ErrorCode{
      VALUE_ERROR = -9,
      UNKNOWN_ERROR = -8,
      BIND_ERROR = -7,
      QUERY_ERROR = -6,
      CONSISTENCY_ERROR = -5,
      PREPARED_SELECT_FAILED = -4,
      PREPARED_INSERT_FAILED = -3,
      SESSION_FAILED = -2,
      SUCCESS = 0,
      NOT_FOUND = 1
    } ErrorCode_t;

    typedef enum InsertMode{
      DEFAULT_BACKLOG_MODE = -1,
      DISALLOW_BACKLOG = 0,
      ALLOW_BACKLOG = 1,
      USE_ONLY_BACKLOG = 2
    } InsertMode_t;

  public:
    // ****************************************************************************************************
    /// @class         Result
    ///
    template<typename RVal_T>
    class Result
    {
      public:
        const ErrorCode_t error_code;
        const std::string result_message;
        const RVal_T data;
        const size_t size;

        Result(ErrorCode_t error_code, std::string result_message, RVal_T data, size_t size = 0):
          error_code(error_code),
          result_message(result_message),
          data(data),
          size(size)
        {}

        Result(const Result &that):
          error_code(that.error_code),
          result_message(that.result_message),
          data(that.data),
          size(that.size)
        {}

        explicit operator bool() const{
          return this->error_code == SUCCESS;
        }
    };

  private:
    CassCluster* cluster;
    CassSession* session;
    const CassPrepared* prepared_insert;
    const CassPrepared* prepared_select;
    std::atomic<bool> is_initialized;
    std::atomic<bool> do_terminate_thread;
    std::deque<std::tuple<Key_T,Val_T,int32_t>> backlog_queue;
    std::mutex backlog_mutex;
    std::thread backlog_thread;
    std::map<std::string, std::string> config;
    InsertMode_t default_backlog_mode;
    std::vector<CassConsistency> read_consistencies;
    std::vector<CassConsistency> write_consistencies;
    const std::map<std::string, std::string> default_config = {
        {"table", "cache.values"},
        {"key_field", "key_field"},
        {"value_field", "value_field"},
        {"username", "username"},
        {"password", "password"},
        {"ip_addresses", "127.0.0.1"},
        {"read_consistencies", "LOCAL_QUORUM, LOCAL_ONE, ONE"},
        {"write_consistencies", "LOCAL_ONE, ONE, ANY"},
        {"client_io_threads", "2"},
        {"client_queue_size", "8192"},
        {"client_server_connects_per_thread", "1"},
        {"client_max_connects_per_thread", "2"},
        {"client_max_conc_connect_creation", "1"},
        {"client_max_concurrent_requests", "100"},
        {"client_log_level", "2"},
        {"default_backlog_mode", "1"}
      };

  private:
    // ****************************************************************************************************
    /// @name            trim
    ///
    /// @brief           Trim the leading and trailing space from a string and return it.
    ///
    /// @param           str
    ///
    /// @return          copy of the trimmed string.
    ///
    static std::string trim(const std::string str){
      std::string s = str;
      s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int c) {
        return not std::isspace(c);
      }));
      s.erase(std::find_if(s.rbegin(), s.rend(), [](int c) {
        return not std::isspace(c);
      }).base(), s.end());
      return s;
    } // trim()

    // ****************************************************************************************************
    /// @name            str_to_int
    ///
    /// @brief           Convert a string to an integer, using a default value if an exception is thrown.
    ///
    /// @param           str
    /// @param           default_value
    ///
    /// @return          The integer value converted from the string, or the default value on an error.
    ///
    static int str_to_int(std::string str, int default_value){
      try{
        return std::stoi(str);
      } // try
      catch(const std::exception& exception){
        return default_value;        
      } // catch
    } // str_to_int()

    // ****************************************************************************************************
    /// @name            configure
    ///
    /// @brief           Configure the connection. The interal configuration must be valid.
    ///
    void configure(void){
      //
      // Set the read/write consistencies
      //
      auto parse_consistencies = [](std::string str){
        std::vector<CassConsistency> consistencies;
        std::stringstream ss(str);
        std::string element;
        while(std::getline(ss, element, ','))
        {
          element = trim(element);
          CassConsistency consistency = element == "ALL"          ? CASS_CONSISTENCY_ALL :
                                        element == "EACH_QUORUM"  ? CASS_CONSISTENCY_EACH_QUORUM :
                                        element == "QUORUM"       ? CASS_CONSISTENCY_QUORUM :
                                        element == "LOCAL_QUORUM" ? CASS_CONSISTENCY_LOCAL_QUORUM :
                                        element == "ONE"          ? CASS_CONSISTENCY_ONE :
                                        element == "TWO"          ? CASS_CONSISTENCY_THREE :
                                        element == "LOCAL_ONE"    ? CASS_CONSISTENCY_LOCAL_ONE :
                                        element == "ANY"          ? CASS_CONSISTENCY_ANY :
                                        element == "SERIAL"       ? CASS_CONSISTENCY_SERIAL :
                                        element == "LOCAL_SERIAL" ? CASS_CONSISTENCY_LOCAL_SERIAL :
                                                                    CASS_CONSISTENCY_UNKNOWN;
          if(consistency != CASS_CONSISTENCY_UNKNOWN){
            consistencies.push_back(consistency);
          } // if
        }
        if(consistencies.size() == 0){
          consistencies.push_back(CASS_CONSISTENCY_ANY);
        } // if
        return consistencies;
      };
      this->read_consistencies = parse_consistencies(config.at("read_consistencies"));
      this->write_consistencies = parse_consistencies(config.at("write_consistencies"));

      //
      // Set the default backlog mode.
      //
      int backlog_mode = str_to_int(config.at("default_backlog_mode"), 1);
      this->default_backlog_mode = backlog_mode == 0 ? DISALLOW_BACKLOG :
                                   backlog_mode == 2 ? USE_ONLY_BACKLOG :
                                                       ALLOW_BACKLOG;

      //
      // Set the log level
      //
      int log_level = str_to_int(config.at("client_log_level"), 2);
      cass_log_set_level(log_level == 0 ? CASS_LOG_DISABLED :
                         log_level == 1 ? CASS_LOG_CRITICAL :
                         log_level == 2 ? CASS_LOG_ERROR :
                         log_level == 3 ? CASS_LOG_WARN :
                         log_level == 4 ? CASS_LOG_INFO :
                         log_level == 5 ? CASS_LOG_DEBUG :
                                          CASS_LOG_TRACE);

      //
      // Create the cluster profile once.
      //
      this->cluster = cass_cluster_new();
      cass_cluster_set_credentials(this->cluster, config.at("username").c_str(), config.at("password").c_str());
      cass_cluster_set_contact_points(this->cluster, config.at("ip_addresses").c_str());
      cass_cluster_set_num_threads_io(this->cluster, str_to_int(config.at("client_io_threads"), 2));
      cass_cluster_set_queue_size_io(this->cluster, str_to_int(config.at("client_queue_size"), 8192));
      cass_cluster_set_core_connections_per_host(this->cluster, str_to_int(config.at("client_server_connects_per_thread"), 1));
      cass_cluster_set_max_connections_per_host(this->cluster, str_to_int(config.at("client_max_connects_per_thread"), 2));
      cass_cluster_set_max_concurrent_creation(this->cluster, str_to_int(config.at("client_max_conc_connect_creation"), 1));
      cass_cluster_set_max_concurrent_requests_threshold(this->cluster, str_to_int(config.at("client_max_concurrent_requests"), 100));

      //
      // Setup SSL (https://docs.datastax.com/en/developer/cpp-driver/2.0/topics/security/ssl/)
      //
      if(config.count("server_trusted_cert") or
         (config.count("client_ssl_cert") and config.count("client_ssl_key"))){
        CassSsl* ssl = cass_ssl_new();

        //
        // Server Certificate
        //
        if(config.count("server_trusted_cert")){
          auto load_trusted_cert_file = [](const char* file, CassSsl* ssl) -> CassError{
            std::ifstream input_file_stream(file);
            std::stringstream string_buffer;
            string_buffer << input_file_stream.rdbuf();
            std::string certificate = string_buffer.str();
            return cass_ssl_add_trusted_cert(ssl, certificate.c_str());
          };
          load_trusted_cert_file("path/server.pem", ssl); // May be repeated more than once for each file.

          int verify_mode = str_to_int(config.at("server_verify_mode"), 1);
          cass_ssl_set_verify_flags(ssl, verify_mode == 0 ? CASS_SSL_VERIFY_NONE :
                                         verify_mode == 2 ? CASS_SSL_VERIFY_PEER_CERT | CASS_SSL_VERIFY_PEER_IDENTITY :
                                         verify_mode == 3 ? CASS_SSL_VERIFY_PEER_CERT | CASS_SSL_VERIFY_PEER_IDENTITY_DNS :
                                                            CASS_SSL_VERIFY_PEER_CERT);
        } // if

        //
        // Client Certificate (Authentication)
        //
        if(config.count("client_ssl_cert") and config.count("client_ssl_key")){
          auto load_ssl_cert = [](const char* file, CassSsl* ssl) -> CassError{
            std::ifstream input_file_stream(file);
            std::stringstream string_buffer;
            string_buffer << input_file_stream.rdbuf();
            std::string certificate = string_buffer.str();
            return cass_ssl_set_cert(ssl, certificate.c_str());
          };
          load_ssl_cert(config.at("client_ssl_cert").c_str(), ssl);
          auto load_private_key = [](const char* file, CassSsl* ssl, const char* key_password) -> CassError{
            std::ifstream input_file_stream(file);
            std::stringstream string_buffer;
            string_buffer << input_file_stream.rdbuf();
            std::string certificate = string_buffer.str();
            return cass_ssl_set_private_key(ssl, certificate.c_str(), key_password);
          };
          load_private_key(config.at("client_ssl_key").c_str(), ssl, 
              config.count("client_key_password") ? config.at("client_key_password").c_str() : nullptr);
        } // if

        cass_cluster_set_ssl(this->cluster, ssl);
        cass_ssl_free(ssl);
      } // if

      //
      // Initialize the connection
      //
      this->initialize();

      //
      // Start the backlog thread
      //
      do_terminate_thread = false;
      this->backlog_thread = std::thread([&](void){
        std::this_thread::sleep_for(std::chrono::seconds(2));
        try{
          while(not do_terminate_thread){
            if(this->is_initialized){
              //
              // Get all the entries in the backlog so we can attempt to send them.
              //
              std::deque<std::tuple<Key_T,Val_T,int32_t>> backlog;
              {
                std::lock_guard<std::mutex> lock( this->backlog_mutex );
                backlog = this->backlog_queue;
                this->backlog_queue.clear();
              }

              //
              // Attempt to process the backlog.
              //
              if(backlog.size() > 0){
                std::vector<std::tuple<Key_T,Val_T,int32_t>> unprocessed;
                for(auto& request : backlog){
                  ValuStor::Result<Val_T> result = this->store(std::get<0>(request), std::get<1>(request), std::get<2>(request), DISALLOW_BACKLOG);
                  if(not result){
                    unprocessed.push_back(request);
                  } // if
                } // for

                //
                // Reinsert the failed requests back into the front of the queue.
                //
                if(unprocessed.size() != 0){
                  std::lock_guard<std::mutex> lock( this->backlog_mutex );
                  this->backlog_queue.insert(this->backlog_queue.begin(), unprocessed.begin(), unprocessed.end());
                } // if
              } // if
            } // if
            else{
              std::this_thread::sleep_for(std::chrono::seconds(8));
            } // else
            std::this_thread::sleep_for(std::chrono::seconds(2));
          } // while
        } // try
        catch(...){}
      });
      backlog_thread.detach();
    } // configure()

  public:
    // ****************************************************************************************************
    /// @name            constructor
    ///
    /// @brief           Create a ValuStor, loading the configuration from the supplied configuration.
    ///
    ValuStor(const std::map<std::string, std::string> configuration):
      cluster(nullptr),
      session(nullptr),
      prepared_insert(nullptr),
      prepared_select(nullptr),
      is_initialized(false),
      do_terminate_thread(false),
      default_backlog_mode(ALLOW_BACKLOG)
    {
      //
      // Use the configuration supplied.
      //
      for(const auto& pair : configuration){
        std::string key = trim(pair.first);
        if(this->default_config.count(key) != 0){
          this->config[key] = pair.second;
        } // if
      } // if

      //
      // Copy in any missing defaults
      //
      for(const auto& pair : this->default_config){
        std::string key = trim(pair.first);
        if(this->config.count(key) == 0){
          this->config[key] = pair.second;
        } // if
      } // for

      //
      // Configure the connection
      //
      this->configure();
    }

    // ****************************************************************************************************
    /// @name            constructor
    ///
    /// @brief           Create a ValuStor, loading the configuration from a file.
    ///
    ValuStor(const std::string config_filename):
      cluster(nullptr),
      session(nullptr),
      prepared_insert(nullptr),
      prepared_select(nullptr),
      is_initialized(false),
      do_terminate_thread(false),
      default_backlog_mode(ALLOW_BACKLOG)
    {
      //
      // Load in the config
      //
      config = default_config;
      try{
        std::ifstream config_file(config_filename);
        std::string line;
        while(std::getline(config_file, line)){
          auto comment_marker = line.find("#");
          if(comment_marker != std::string::npos) {
            line = line.substr(0, comment_marker);
          } // if

          auto equal_marker = line.find("=");
          if(equal_marker != std::string::npos){
            this->config[trim(line.substr(0, equal_marker))] = trim(line.substr(equal_marker + 1));
          } // if
        } // while
      } // try
      catch(const std::exception& exception){}

      //
      // Configure the connection
      //
      this->configure();
    }

    ~ValuStor(void)
    {
      do_terminate_thread = true;
      this->is_initialized = false;
      if(this->prepared_select != nullptr){
        cass_prepared_free(this->prepared_select);
      } // if
      if(this->prepared_insert != nullptr){
        cass_prepared_free(this->prepared_insert);
      } // if
      if(this->session != nullptr){
        cass_session_free(this->session);
      } // if
      if(this->cluster != nullptr){
        cass_cluster_free(this->cluster);
      } // if
    }

  private:
    static CassError bind(CassStatement* stmt, size_t index, const int8_t& value) {
      return cass_statement_bind_int8(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const int16_t& value) {
      return cass_statement_bind_int16(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const int32_t& value) {
      return cass_statement_bind_int32(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const uint32_t& value) {
      return cass_statement_bind_uint32(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const int64_t& value) {
      return cass_statement_bind_int64(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const float& value) {
      return cass_statement_bind_float(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const double& value) {
      return cass_statement_bind_double(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const std::string& value) {
      return cass_statement_bind_string_n(stmt, index, value.c_str(), value.size());
    }
    static CassError bind(CassStatement* stmt, size_t index, const char* value) {
      return cass_statement_bind_string_n(stmt, index, value, std::strlen(value));
    }
    static CassError bind(CassStatement* stmt, size_t index, const cass_bool_t& value) {
      return cass_statement_bind_bool(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const CassUuid& value) {
      return cass_statement_bind_uuid(stmt, index, value);
    }
    static CassError bind(CassStatement* stmt, size_t index, const cass_byte_t* value) {
      return cass_statement_bind_bytes(stmt, index, value, std::strlen((char*)value));
    }

    static CassError get(const CassValue* value, int8_t* target, size_t* size){
      *size = 1;
      return cass_value_get_int8(value, target);
    }
    static CassError get(const CassValue* value, int16_t* target, size_t* size){
      *size = 2;
      return cass_value_get_int16(value, target);
    }
    static CassError get(const CassValue* value, int32_t* target, size_t* size){
      *size = 4;
      return cass_value_get_int32(value, target);
    }
    static CassError get(const CassValue* value, uint32_t* target, size_t* size){
      *size = 4;
      return cass_value_get_uint32(value, target);
    }
    static CassError get(const CassValue* value, int64_t* target, size_t* size){
      *size = 8;
      return cass_value_get_int64(value, target);
    }
    static CassError get(const CassValue* value, float* target, size_t* size){
      *size = 4;
      return cass_value_get_float(value, target);
    }
    static CassError get(const CassValue* value, double* target, size_t* size){
      *size = 8;
      return cass_value_get_double(value, target);
    }
    static CassError get(const CassValue* value, std::string* target, size_t* size){
      const char* str;
      size_t str_length;
      CassError error = cass_value_get_string(value, &str, &str_length);
      *target = std::string(str, str + str_length);
      *size = str_length;
      return error;
    }
    static CassError get(const CassValue* value, char** target, size_t* size){
      const char* str;
      size_t str_length;
      CassError error = cass_value_get_string(value, &str, &str_length);
      *target = str;
      *size = str_length;
      return error;
    }
    static CassError get(const CassValue* value, cass_bool_t* target, size_t* size){
      return cass_value_get_bool(value, target);
    }
    static CassError get(const CassValue* value, CassUuid* target, size_t* size){
      return cass_value_get_uuid(value, target);
    }
    static CassError get(const CassValue* value, cass_byte_t** target, size_t* size){
      size_t array_length;
      CassError error = cass_value_get_bytes(value, target, &array_length);
      *size = array_length;
      return error;
    }



  private:
    // ****************************************************************************************************
    /// @name            logFutureErrorMessage
    ///
    static std::string logFutureErrorMessage(CassFuture* future, const std::string& description){
      const char* message;
      size_t message_length;
      if(future != nullptr){
        cass_future_error_message(future, &message, &message_length);
      } // if
      return "Scylla Error: " + description + ": '" + std::string(message, message_length) + "'";
    }

    // ****************************************************************************************************
    /// @name            initialize
    ///
    /// @brief           Initialize the connection given a valid CassCluster* already setup.
    ///
    void initialize(void){
      //
      // Only initialize once.
      //
      if(not this->is_initialized){
        this->session = cass_session_new();
        CassFuture* connect_future = cass_session_connect(this->session, cluster);
        if(connect_future != nullptr){
          cass_future_wait_timed(connect_future, 4000000L); // Wait up to 4s
          if (cass_future_error_code(connect_future) == CASS_OK) {
            //
            // Build the INSERT prepared statement
            //
            {
              std::string statement = "INSERT INTO " + this->config.at("table") + " (" + this->config.at("key_field") +
                                      ", " + this->config.at("value_field") + ") VALUES (?, ?) USING TTL ?";
              CassFuture* future = cass_session_prepare(this->session, statement.c_str());
              if(future != nullptr){
                cass_future_wait_timed(future, 2000000L); // Wait up to 2s
                if (cass_future_error_code(future) == CASS_OK) {
                  this->prepared_insert = cass_future_get_prepared(future);
                } // if
                else{
                  //
                  // Error: Unable to prepare insert statement
                  //
                } // else
                cass_future_free(future);
              } // if
            }

            //
            // Build the SELECT prepared statement
            //
            {
              std::string statement = "SELECT " + this->config.at("value_field") + " FROM " + this->config.at("table") +
                                      " WHERE " + this->config.at("key_field") + "=?";
              CassFuture* future = cass_session_prepare(this->session, statement.c_str());
              if(future != nullptr){
                cass_future_wait_timed(future, 2000000L); // Wait up to 2s
                if (cass_future_error_code(future) == CASS_OK) {
                  this->prepared_select = cass_future_get_prepared(future);
                } // if
                else{
                  //
                  // Error: Unable to prepare select statement
                  //
                } // else
                cass_future_free(future);
              } // if
            }
          } // if
          else{
            //
            // Error: Unable to connect
            //
          } // else
          cass_future_free(connect_future);

          if(this->session != nullptr and this->prepared_insert != nullptr and this->prepared_select != nullptr){
            this->is_initialized = true;
          } // if
          else{
            if(this->prepared_insert != nullptr){
              cass_prepared_free(this->prepared_insert);
              this->prepared_insert = nullptr;
            } // if
            if(this->prepared_select != nullptr){
              cass_prepared_free(this->prepared_select);
              this->prepared_select = nullptr;
            } // if
            if(this->session != nullptr){
              cass_session_free(this->session);
              this->session = nullptr;
            } // if
          } // else
        } // if
      } // if
    } // initialize

  public:
    // ****************************************************************************************************
    /// @name            retrieve
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           key
    ///
    /// @return          If 'result.first', the string value in 'result.second', otherwise not found.
    ///
    ValuStor::Result<Val_T> retrieve(const Key_T& key){
      ErrorCode_t error_code = ValuStor::UNKNOWN_ERROR;
      std::string error_message = "Scylla Error";
      size_t size = 0;
      Val_T data;

      if(this->prepared_select == nullptr){
        error_code = ValuStor::PREPARED_SELECT_FAILED;
        error_message = "Scylla Error: Prepared Select Failed";
      } // else if
      else{
        CassStatement* statement = cass_prepared_bind(this->prepared_select);
        if(statement != nullptr){
          CassError error = bind(statement, 0, key);
          if(error != CASS_OK){
            error_code = ValuStor::BIND_ERROR;
            error_message = "Scylla Error: Unable to bind parameters: " + std::string(cass_error_desc(error));
          } // if
          else{
            for(const auto& level : this->read_consistencies){
              error = cass_statement_set_consistency(statement, level);
              if(error != CASS_OK){
                error_code = ValuStor::CONSISTENCY_ERROR;
                error_message = "Scylla Error: Unable to set statement consistency: " + std::string(cass_error_desc(error));
              } // if
              else{
                CassFuture* result_future = cass_session_execute(this->session, statement);
                if(result_future != nullptr){
                  cass_future_wait_timed(result_future, 2000000L); // Wait up to 2s
                  if (cass_future_error_code(result_future) != CASS_OK) {
                    error_code = ValuStor::QUERY_ERROR;
                    error_message = logFutureErrorMessage(result_future, "Unable to run query");
                  } // if
                  else{
                    const CassResult* cass_result = cass_future_get_result(result_future);
                    if(cass_result != nullptr){
                      const CassRow* row = cass_result_first_row(cass_result);
                      if (row != nullptr) {
                        const CassValue* value = cass_row_get_column(row, 0);
                        CassError error = get(value, &data, &size);
                        if(error != CASS_OK){
                          error_code = ValuStor::VALUE_ERROR;
                          error_message = "Scylla Error: Unable to get the value: " + std::string(cass_error_desc(error));
                        } // if
                        else{
                          error_code = ValuStor::SUCCESS;
                          error_message = "Value read successfully";
                        } // else
                      } // if
                      else{
                        error_code = ValuStor::NOT_FOUND;
                        error_message = "Error: Value Not Found";
                      } // else
                      cass_result_free(cass_result);
                      cass_future_free(result_future); // Free it here, because we break out of the for loop.
                      break;
                    } // if
                  } // else
                  cass_future_free(result_future);
                } // if
              } // else
            } // for
          } // else
          cass_statement_free(statement);
        } // if
      } // else

      return ValuStor::Result<Val_T>(error_code, error_message, data, size);

    }

    // ****************************************************************************************************
    /// @name            store
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           key
    /// @param           value
    ///
    /// @return          'true' if successful, 'false' otherwise.
    ///
    ValuStor::Result<Val_T> store(const Key_T& key, const Val_T& value, int32_t seconds_ttl = 0, InsertMode_t insert_mode = DEFAULT_BACKLOG_MODE){
      if(insert_mode == DEFAULT_BACKLOG_MODE){
        insert_mode = this->default_backlog_mode;
      } // if
      ErrorCode_t error_code = ValuStor::UNKNOWN_ERROR;
      std::string error_message = "Scylla Error";

      if(insert_mode == USE_ONLY_BACKLOG){
        std::lock_guard<std::mutex> lock(this->backlog_mutex);
        this->backlog_queue.push_back(std::tuple<Key_T,Val_T,int32_t>(key, value, seconds_ttl));
      } // if
      else if(this->prepared_insert == nullptr){
        error_code = ValuStor::PREPARED_INSERT_FAILED;
        error_message = "Scylla Error: Prepared Insert Failed";
      } // else if
      else{
        CassStatement* statement = cass_prepared_bind(this->prepared_insert);
        if(statement != nullptr){
          CassError error = ValuStor::bind(statement, 0, key);
          if(error == CASS_OK){
            error = ValuStor::bind(statement, 1, value);
          } // if
          if(error == CASS_OK){
            error = ValuStor::bind(statement, 2, seconds_ttl);
          } // if
          if(error != CASS_OK){
            error_code = ValuStor::BIND_ERROR;
            error_message = "Scylla Error: Unable to bind parameters: " + std::string(cass_error_desc(error));
          } // if
          else{
            for(const auto& level : this->write_consistencies){
              error = cass_statement_set_consistency(statement, level);
              if(error != CASS_OK){
                error_code = ValuStor::CONSISTENCY_ERROR;
                error_message = "Scylla Error: Unable to set statement consistency: " + std::string(cass_error_desc(error));
              } // if
              else{
                CassFuture* result_future = cass_session_execute(this->session, statement);
                if(result_future != nullptr){
                  cass_future_wait_timed(result_future, 2000000L); // Wait up to 2s
                  if (cass_future_error_code(result_future) != CASS_OK) {
                    error_code = ValuStor::QUERY_ERROR;
                    error_message = logFutureErrorMessage(result_future, "Unable to run query");
                  } // if
                  else{
                    error_code = ValuStor::SUCCESS;
                    error_message = "Value stored successfully";
                    cass_future_free(result_future);  // Free it here after a successful insertion, but prior to breaking out of the loop.
                    break;
                  } // else
                  cass_future_free(result_future);
                } // if
              } // else
            } // for
          } // else
          cass_statement_free(statement);
        } // if
      } // else

      if(error_code != ValuStor::SUCCESS and insert_mode == ALLOW_BACKLOG){
        std::lock_guard<std::mutex> lock(this->backlog_mutex);
        this->backlog_queue.push_back(std::tuple<Key_T,Val_T,int32_t>(key, value, seconds_ttl));
      } // if

      return ValuStor::Result<Val_T>(error_code, error_message, value);

    }
};

#endif /* #ifndef VALUE_STORE_H */
