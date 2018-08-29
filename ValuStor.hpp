/*
ValuStor - Scylla DB key-value pair storage

version 1.0.3

Licensed under the MIT License

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
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

// See https://github.com/datastax/cpp-driver/releases
// This has been tested with version 2.7.1.
#include <cassandra.h>

namespace ValuStor{

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

//
// C++11 does not have C++14's make_index_sequence/make_integer_sequence.
// Manual template metaprogramming is required.
// The following is used to construct a sequence of indices.
//
// For Example:
//   S<7>
//     : I<{0}, S<6>>
//     : I<{0}, I<{0}, S<5>>>
//     : I<{0}, I<{0}, I<{0}, S<4>>>>
//     : I<{0}, I<{0}, I<{0}, I<{0}, S<3>>>>>
//     : I<{0}, I<{0}, I<{0}, I<{0}, I<{0}, S<2>>>>>>
//     : I<{0}, I<{0}, I<{0}, I<{0}, I<{0}, I<{0}, S<1>>>>>>>
//     : I<{0}, I<{0}, I<{0}, I<{0}, I<{0}, I<{0}, {0}>>>>>>
//     : I<{0}, I<{0}, I<{0}, I<{0}, I<{0}, {0, 1}>>>>>
//     : I<{0}, I<{0}, I<{0}, I<{0}, {0, 1, 2}>>>>
//     : I<{0}, I<{0}, I<{0}, {0, 1, 2, 3}>>>
//     : I<{0}, I<{0}, {0, 1, 2, 3, 4}>>
//     : I<{0}, {0, 1, 2, 3, 4, 5}>
//     : {0, 1, 2, 3, 4, 5, 6}
//   Where,
//       S = Sequencer
//       I = Indexer
//    {..} = Indices
//
// Usage:
//  auto sequence = typename Sequencer<std::tuple_size<std::tuple<Keys...>>{}>::Indices{};
//

// The Indices are a list of {0, 1, .., n-1}
template<size_t...> struct Indices{};

// The Indexer is the "reducer", combining two sets of Indices into one set of Indices, one right after the other.
template<typename, typename> struct Indexer;
template<size_t... A, size_t... B> struct Indexer<Indices<A...>, Indices<B...>> : Indices<A...,(1+B)...>{};

// The Sequencer is the "mapper", splitting a count into two separate indices for indexing by the Indexer.
// These indices are sequenced recursively.
//
// Primary Template (recursive case):
//   T=# of elements in the parent Indices
template<size_t T> struct Sequencer : Indexer<Indices<0>, typename Sequencer<T-1>::Indices>{};
//
// Specialization (base/terminating case):
template<> struct Sequencer<1> : Indices<0>{};

// ****************************************************************************************************
/// @class         ValuStorUUIDGen
///
/// @brief         Contains a CassUuidGen* that is used to globally generate CassUuid.
///                According to the documentation it is thread-safe and there should be one per application.
///
class ValuStorUUIDGen{
  public:
    CassUuidGen* const uuid_gen;

  private:
    ValuStorUUIDGen(void):
      uuid_gen(cass_uuid_gen_new())
    {}
    ~ValuStorUUIDGen(void){
      cass_uuid_gen_free(this->uuid_gen);
    }

  public:
    static ValuStorUUIDGen& instance(void){
      static ValuStorUUIDGen generator;
      return generator;
    }
};

// ****************************************************************************************************
/// @class         ValuStor
///
/// @brief         A base object class from which all top level classes may inherit.
///                An object implements enough basic methods to support rudimentary object reflection.
///
template<typename Val_T, typename... Keys>
class ValuStor
{
  public:
    // ****************************************************************************************************
    /// @class         Result
    ///
    class Result
    {
      friend class ValuStor;

      public:
        const ErrorCode_t error_code;
        const std::string result_message;
        const Val_T data;
        std::vector<std::pair<Val_T, std::tuple<Keys...>>> results;

      private:
        // ****************************************************************************************************
        /// @name            Result
        ///
        /// @brief           Construct a Result object
        ///
        Result(ErrorCode_t error_code, std::string result_message, Val_T result_data, std::tuple<Keys...>&& keys):
          error_code(error_code),
          result_message(result_message),
          data(result_data),
          results({std::make_pair(this->data, std::move(keys))})
        {}

        // ****************************************************************************************************
        /// @name            Result
        ///
        /// @brief           Construct a Result object
        ///
        Result(ErrorCode_t error_code, std::string result_message, std::vector<std::pair<Val_T, std::tuple<Keys...>>>&& resulting_data):
          error_code(error_code),
          result_message(result_message),
          data(resulting_data.size() > 0 ? resulting_data.at(0).first : Val_T{}),
          results(std::move(resulting_data))
        {}

      public:
        Result(void) = default;
        ~Result(void) = default;
        Result(const Result& that) = default;
        Result(Result&& that) = default;
        Result& operator=(const Result&) = default;
        Result& operator=(Result&&) = default;

        // ****************************************************************************************************
        /// @name            bool()
        ///
        /// @brief           'true' if the result was successful, 'false' if it was not.
        ///
        explicit operator bool() const{
          return this->error_code == SUCCESS;
        }
    };

  private:
    CassCluster* cluster;
    CassSession* session;
    const CassPrepared* prepared_insert;
    std::map<size_t, const CassPrepared*> prepared_selects;

    std::atomic<bool> is_initialized;
    std::thread backlog_thread;
    InsertMode_t default_backlog_mode;
    std::vector<CassConsistency> read_consistencies;
    std::vector<CassConsistency> write_consistencies;
    std::atomic<bool>* do_terminate_thread_ptr;
    std::shared_ptr<std::atomic<bool>> is_processing_backlog_ptr;
    std::mutex* backlog_mutex_ptr;
    std::deque<std::tuple<std::tuple<Keys...>,Val_T,int32_t,int64_t>>* backlog_queue_ptr;
    std::vector<std::string> keys;

    std::map<std::string, std::string> config;
    const std::map<std::string, std::string> default_config = {
        {"table", "cache.values"},
        {"key_field", "key_field"},
        {"value_field", "value_field"},
        {"username", "username"},
        {"password", "password"},
        {"hosts", "127.0.0.1"},
        {"port", "9042"},
        {"read_consistencies", "LOCAL_QUORUM, LOCAL_ONE, ONE"},
        {"write_consistencies", "LOCAL_ONE, ONE, ANY"},
        {"client_io_threads", "2"},
        {"client_queue_size", "8192"},
        {"client_server_connects_per_thread", "1"},
        {"client_max_connects_per_thread", "2"},
        {"client_max_conc_connect_creation", "1"},
        {"client_max_concurrent_requests", "100"},
        {"client_log_level", "2"},
        {"default_backlog_mode", "1"},
        {"server_trusted_cert", ""},
        {"server_verify_mode", "0"},
        {"client_ssl_cert", ""},
        {"client_ssl_key", ""},
        {"client_key_password", ""}
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
      }
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
          }
        }
        if(consistencies.size() == 0){
          consistencies.push_back(CASS_CONSISTENCY_ANY);
        }
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
      // Retrieve the keys
      //
      std::stringstream ss(this->config.at("key_field"));
      std::string field;
      while(std::getline(ss, field, ',')){
        field = trim(field);
        if(field != ""){
          keys.push_back(field);
        }
      }

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
      std::string username = config.at("username");
      std::string password = config.at("password");
      if(username != "" or password != ""){
        cass_cluster_set_credentials(this->cluster, username.c_str(), password.c_str());
      }
      cass_cluster_set_contact_points(this->cluster, config.at("hosts").c_str());
      cass_cluster_set_port(this->cluster, str_to_int(config.at("port"), 9042));
      cass_cluster_set_num_threads_io(this->cluster, str_to_int(config.at("client_io_threads"), 2));
      cass_cluster_set_queue_size_io(this->cluster, str_to_int(config.at("client_queue_size"), 8192));
      cass_cluster_set_core_connections_per_host(this->cluster, str_to_int(config.at("client_server_connects_per_thread"), 1));
      cass_cluster_set_max_connections_per_host(this->cluster, str_to_int(config.at("client_max_connects_per_thread"), 2));
      cass_cluster_set_max_concurrent_creation(this->cluster, str_to_int(config.at("client_max_conc_connect_creation"), 1));
      cass_cluster_set_max_concurrent_requests_threshold(this->cluster, str_to_int(config.at("client_max_concurrent_requests"), 100));

      //
      // Setup SSL (https://docs.datastax.com/en/developer/cpp-driver/2.0/topics/security/ssl/)
      //
      std::string server_trusted_cert = config.count("server_trusted_cert") ? config.at("server_trusted_cert") : "";
      std::string client_ssl_cert = config.count("client_ssl_cert") ? config.at("client_ssl_cert") : "";
      std::string client_ssl_key = config.count("client_ssl_key") ? config.at("client_ssl_key") : "";

      if(server_trusted_cert != "" or
         (client_ssl_cert != "" and client_ssl_key != "")){
        CassSsl* ssl = cass_ssl_new();

        //
        // Server Certificate
        //
        if(server_trusted_cert != ""){
          auto load_trusted_cert_file = [](const char* file, CassSsl* ssl) -> CassError{
            std::ifstream input_file_stream(file);
            std::stringstream string_buffer;
            string_buffer << input_file_stream.rdbuf();
            std::string certificate = string_buffer.str();
            return cass_ssl_add_trusted_cert(ssl, certificate.c_str());
          };
          std::stringstream ss(server_trusted_cert);
          std::string cert;
          while(std::getline(ss, cert, ','))
          {
            cert = trim(cert);
            if(cert != ""){
              load_trusted_cert_file(cert.c_str(), ssl);
            }
          }

          int verify_mode = str_to_int(config.at("server_verify_mode"), 1);
          cass_ssl_set_verify_flags(ssl, verify_mode == 0 ? CASS_SSL_VERIFY_NONE :
                                         verify_mode == 2 ? CASS_SSL_VERIFY_PEER_CERT | CASS_SSL_VERIFY_PEER_IDENTITY :
                                         verify_mode == 3 ? CASS_SSL_VERIFY_PEER_CERT | CASS_SSL_VERIFY_PEER_IDENTITY_DNS :
                                                            CASS_SSL_VERIFY_PEER_CERT);
        }

        //
        // Client Certificate (Authentication)
        //
        if(client_ssl_cert != "" and client_ssl_key != ""){
          auto load_ssl_cert = [](const char* file, CassSsl* ssl) -> CassError{
            std::ifstream input_file_stream(file);
            std::stringstream string_buffer;
            string_buffer << input_file_stream.rdbuf();
            std::string certificate = string_buffer.str();
            return cass_ssl_set_cert(ssl, certificate.c_str());
          };
          load_ssl_cert(client_ssl_cert.c_str(), ssl);
          auto load_private_key = [](const char* file, CassSsl* ssl, const char* key_password) -> CassError{
            std::ifstream input_file_stream(file);
            std::stringstream string_buffer;
            string_buffer << input_file_stream.rdbuf();
            std::string certificate = string_buffer.str();
            return cass_ssl_set_private_key(ssl, certificate.c_str(), key_password);
          };
          load_private_key(client_ssl_key.c_str(), ssl,
              config.count("client_key_password") and config.at("client_key_password") != "" ?
                        config.at("client_key_password").c_str() : nullptr);
        }

        cass_cluster_set_ssl(this->cluster, ssl);
        cass_ssl_free(ssl);
      }
    } // configure()

    // ****************************************************************************************************
    /// @name            run_backlog_thread
    ///
    /// @brief           Run the backlog thread.
    ///
    void run_backlog_thread(void){
      //
      // Start the backlog thread
      //
      std::atomic<bool> are_pointers_setup(false);

      // *****************************************************************************
      /// @name           initialize
      ///
      /// @brief          Initialize the connection given a valid CassCluster* already setup.
      ///
      auto initialize = [&](void){
        this->session = cass_session_new();
        if(this->session != nullptr){
          CassFuture* connect_future = cass_session_connect(this->session, cluster);
          if(connect_future != nullptr){
            cass_future_wait_timed(connect_future, 4000000L); // Wait up to 4s
            if (cass_future_error_code(connect_future) == CASS_OK) {
              //
              // Build the INSERT prepared statement
              //
              {
                std::string statement = "INSERT INTO " + this->config.at("table") + " (" + this->config.at("key_field") +
                                        ", " + this->config.at("value_field") + ") VALUES (";
                for(size_t ndx = 0; ndx < keys.size(); ndx++){
                  statement += "?,";
                }
                statement += "?) USING TTL ?";

                CassFuture* future = cass_session_prepare(this->session, statement.c_str());
                if(future != nullptr){
                  cass_future_wait_timed(future, 2000000L); // Wait up to 2s
                  if (cass_future_error_code(future) == CASS_OK) {
                    this->prepared_insert = cass_future_get_prepared(future);
                  }
                  else{
                    //
                    // Error: Unable to prepare insert statement
                    //
                  }
                  cass_future_free(future);
                }
              }

              //
              // Build the SELECT prepared statements
              //
              {
                for(size_t total = 1; total <= keys.size(); total++){
                  // total == # of keys to move to the WHERE clause.
                  std::string statement = "SELECT " + this->config.at("value_field");
                  for(size_t value = total; value < keys.size(); value++){
                    statement += "," + keys.at(value);
                  }
                  statement += " FROM " + this->config.at("table") + " WHERE ";
                  for(size_t key = 0; key < total; key++){
                    statement += (key != 0 ? " AND " : "") + keys.at(key) + "=?";
                  }

                  CassFuture* future = cass_session_prepare(this->session, statement.c_str());
                  if(future != nullptr){
                    cass_future_wait_timed(future, 2000000L); // Wait up to 2s
                    if (cass_future_error_code(future) == CASS_OK) {
                      this->prepared_selects[total] = cass_future_get_prepared(future);
                    }
                    else{
                      //
                      // Error: Unable to prepare select statement
                      //
                    }
                    cass_future_free(future);
                  }
                }
              }
            }
            else{
              //
              // Error: Unable to connect
              //
            }
            cass_future_free(connect_future);

            if(this->session != nullptr and this->prepared_insert != nullptr and this->prepared_selects.size() != 0){
              this->is_initialized = true;
            }
            else{
              if(this->prepared_insert != nullptr){
                cass_prepared_free(this->prepared_insert);
                this->prepared_insert = nullptr;
              }
              for(const auto& pair : this->prepared_selects){
                if(pair.second != nullptr){
                  cass_prepared_free(pair.second);
                }
              }
              this->prepared_selects.clear();
            }
          }
          if(not this->is_initialized and this->session != nullptr){
            cass_session_free(this->session);
            this->session = nullptr;
          }
        }        
      };

      // *****************************************************************************
      /// @name           backlog_thread
      ///
      /// @brief          The backlog thread handles initializing the connection and manages the backlog queue.
      ///
      this->backlog_thread = std::thread([&, initialize](void){
        //
        // Setup the pointers back to the master thread.
        // The backlog thread will never close until the master thread tells it to close.
        // At that point it can terminate after the master thread, so it must control the data to prevent seg faults.
        //
        std::atomic<bool> do_terminate_thread(false);
        std::mutex backlog_mutex;
        std::shared_ptr<std::atomic<bool>> is_processing_backlog = this->is_processing_backlog_ptr;
        std::deque<std::tuple<std::tuple<Keys...>,Val_T,int32_t,int64_t>> backlog_queue;

        this->do_terminate_thread_ptr = &do_terminate_thread;
        this->backlog_mutex_ptr = &backlog_mutex;
        this->backlog_queue_ptr = &backlog_queue;

        //
        // This tells the master thread that we are processing.
        // It will not go out of scope until we set processing to 'false', allowing both threads to poll is_initialized without locking.
        //
        *is_processing_backlog = true;

        //
        // Notify the master thread that the pointers are setup and it is okay for 'are_pointers_setup' to go out-of-scope.
        //
        are_pointers_setup = true;

        //
        // Wait for the thread to initialize.
        // During this period, the master thread cannot destruct since we are still accessing "is_initialized"
        // "do_terminate_thread" must be set first.
        // Once initialization takes place, we never need to access it again.
        //
        initialize();
        while(not do_terminate_thread and not this->is_initialized){
          try{
            std::this_thread::sleep_for(std::chrono::seconds(1));
            initialize();
          }
          catch(...){}
        }
        *is_processing_backlog = false;
        while(not do_terminate_thread){
          try{
            std::this_thread::sleep_for(std::chrono::seconds(2));

            //
            // Get all the entries in the backlog so we can attempt to send them.
            //
            std::deque<std::tuple<std::tuple<Keys...>,Val_T,int32_t,int64_t>> backlog;
            {
              std::lock_guard<std::mutex> lock( backlog_mutex );
              backlog = backlog_queue;
              backlog_queue.clear();
              *is_processing_backlog = backlog.size() > 0 and not do_terminate_thread;
            }

            //
            // Attempt to process the backlog.
            //
            if(*is_processing_backlog){
              std::vector<std::tuple<std::tuple<Keys...>,Val_T,int32_t,int64_t>> unprocessed;
              for(auto& request : backlog){
                if(not do_terminate_thread){
                  //
                  // Proxy the call to store().
                  // We need to unpack the tuple<Keys...> into paramters to store().
                  //
                  ValuStor::Result result = this->call_store(std::get<0>(request), // tuple<Keys...>
                                                             std::get<1>(request), // Val_T
                                                             std::get<2>(request), // TTL
                                                             DISALLOW_BACKLOG,     // InsertMode
                                                             std::get<3>(request), // insert time
                                                             typename Sequencer<std::tuple_size<std::tuple<Keys...>>{}>::Indices{});
                  if(not result){
                    unprocessed.push_back(request);
                  }
                }
              }

              //
              // Reinsert the failed requests back into the front of the queue.
              //
              if(unprocessed.size() != 0){
                std::lock_guard<std::mutex> lock( backlog_mutex );
                if(not do_terminate_thread){
                  backlog_queue.insert(backlog_queue.begin(), unprocessed.begin(), unprocessed.end());
                }
              }
            }

            //
            // Let the master process know that a backlog is no longer running.
            //
            *is_processing_backlog = false;
          }
          catch(...){}
        }

        //
        // Acquire a lock one last time to ensure that the master thread isn't using the lock in the destructor.
        //
        {
          std::lock_guard<std::mutex> lock( backlog_mutex );
        }
      });

      //
      // Wait for the thread to initialize and detach it from this thread.
      //
      backlog_thread.detach();
      while(not are_pointers_setup){
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    } // run_backlog_thread()

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
      is_initialized(false),
      default_backlog_mode(ALLOW_BACKLOG),
      do_terminate_thread_ptr(nullptr),
      is_processing_backlog_ptr(new std::atomic<bool>(false)),
      backlog_mutex_ptr(nullptr),
      backlog_queue_ptr(nullptr)
    {
      //
      // Use the configuration supplied.
      //
      for(const auto& pair : configuration){
        std::string key = trim(pair.first);
        if(this->default_config.count(key) != 0){
          this->config[key] = pair.second;
        }
      }

      //
      // Add in any missing defaults
      //
      for(const auto& pair : this->default_config){
        std::string key = trim(pair.first);
        if(this->config.count(key) == 0){
          this->config[key] = pair.second;
        }
      }

      //
      // Configure the connection
      //
      this->configure();

      //
      // Start the backlog thread (which will perform initialization)
      //
      this->run_backlog_thread();
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
      is_initialized(false),
      default_backlog_mode(ALLOW_BACKLOG),
      do_terminate_thread_ptr(nullptr),
      is_processing_backlog_ptr(new std::atomic<bool>(false)),
      backlog_mutex_ptr(nullptr),
      backlog_queue_ptr(nullptr)
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
          }

          auto equal_marker = line.find("=");
          if(equal_marker != std::string::npos){
            this->config[trim(line.substr(0, equal_marker))] = trim(line.substr(equal_marker + 1));
          }
        }
      }
      catch(const std::exception& exception){}

      //
      // Configure the connection
      //
      this->configure();

      //
      // Start the backlog thread (which will perform initialization)
      //
      this->run_backlog_thread();
    }

    // ****************************************************************************************************
    /// @name            destructor
    ///
    /// @brief           Close the connection, the backlog thread, and free up allocations.
    ///
    ///
    ~ValuStor(void)
    {
      //
      // Terminate the backlog thread, but be careful as state is shared.
      // By terminating the thread and clearing the queue while holding the lock, we can prevent most race conditions.
      //
      bool was_backlog_running = false;
      {
        std::lock_guard<std::mutex> lock(*this->backlog_mutex_ptr);
        was_backlog_running = *this->is_processing_backlog_ptr;
        this->backlog_queue_ptr->clear();
        *this->do_terminate_thread_ptr = true;
      }

      //
      // If the backlog thread was processing, we must wait for it to finish so it isn't accessing any of this' data.
      // When it isn't in processing, it cannot reenter processing without knowing that "do_terminate_thread" was set.
      // If the backlog was not running, it is impossible for there to be a race condition.
      //
      if(was_backlog_running){
        while(*this->is_processing_backlog_ptr){
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
      }

      //
      // Close up the cassandra connection.
      //
      for(const auto& pair : this->prepared_selects){
        if(pair.second != nullptr){
          cass_prepared_free(pair.second);
        }
      }
      this->prepared_selects.clear();
      if(this->prepared_insert != nullptr){
        cass_prepared_free(this->prepared_insert);
      }
      if(this->session != nullptr){
        cass_session_free(this->session);
      }
      if(this->cluster != nullptr){
        cass_cluster_free(this->cluster);
      }
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
      }
      return "Scylla Error: " + description + ": '" + std::string(message, message_length) + "'";
    }

  public:
    // ****************************************************************************************************
    /// @name            retrieve
    ///
    /// @brief           Get the value associated with the provided key.
    ///
    /// @param           keys
    /// @param           count
    ///
    /// @return          If 'result.first', the string value in 'result.second', otherwise not found.
    ///
    ValuStor::Result retrieve(Keys... keys, size_t count = 0){
      ErrorCode_t error_code = UNKNOWN_ERROR;
      std::string error_message = "Scylla Error";

      std::vector<std::pair<Val_T, std::tuple<Keys...>>> retrieved_data;

      if(not this->is_initialized){
        error_code = SESSION_FAILED;
        error_message = "Scylla Error: Could not connect to server(s)";
      }
      else if(this->prepared_selects.size() == 0){
        error_code = PREPARED_SELECT_FAILED;
        error_message = "Scylla Error: Prepared Select Failed";
      }
      else{
        //
        // count = # of keys in the WHERE clause, or '0' for all.
        // -----
        //   1: SELECT v,k2,k3 FROM cache.tbl131 WHERE k1=?
        //   2: SELECT v,k3 FROM cache.tbl131 WHERE k1=? AND k2=?
        //   3: SELECT v FROM cache.tbl131 WHERE k1=? AND k2=? AND k3=?
        //
        auto prepared_select = this->prepared_selects.count(count) != 0 ? this->prepared_selects.at(count) :
                                                                          this->prepared_selects.rbegin()->second;
        CassStatement* statement = cass_prepared_bind(prepared_select);
        if(statement != nullptr){
          std::pair<CassError, size_t> bind_error = bind(statement, 0, keys...);
          CassError error = bind_error.first;
          if(error != CASS_OK and false){
            error_code = BIND_ERROR;
            error_message = "Scylla Error: Unable to bind parameters: " + std::string(cass_error_desc(error));
          }
          else{
            for(const auto& level : this->read_consistencies){
              error = cass_statement_set_consistency(statement, level);
              if(error != CASS_OK){
                error_code = CONSISTENCY_ERROR;
                error_message = "Scylla Error: Unable to set statement consistency: " + std::string(cass_error_desc(error));
              }
              else{
                CassFuture* result_future = cass_session_execute(this->session, statement);
                if(result_future != nullptr){
                  cass_future_wait_timed(result_future, 2000000L); // Wait up to 2s
                  if (cass_future_error_code(result_future) != CASS_OK) {
                    error_code = QUERY_ERROR;
                    error_message = logFutureErrorMessage(result_future, "Unable to run query");
                  }
                  else{
                    const CassResult* cass_result = cass_future_get_result(result_future);
                    if(cass_result != nullptr){
                      size_t row_count = cass_result_row_count(cass_result);
                      if(row_count != 0){
                        CassIterator* iterator = cass_iterator_from_result(cass_result);
                        while (cass_iterator_next(iterator)) {
                          //
                          // We have a row of data to process
                          //
                          const CassRow* row = cass_iterator_get_row(iterator);
                          if (row != nullptr) {
                            CassError error = CASS_OK;
                            Val_T data_gotten{};
                            const CassValue* value = cass_row_get_column(row, 0);
                            if(value != nullptr){
                              error = get(value, &data_gotten); // Get the Val_T response.
                              if(error == CASS_OK){
                                error = get(row, 1, count, &keys...); // Get the Key(s) that may have optionally been SELECTed.
                              }
                              if(error == CASS_OK){
                                retrieved_data.push_back(std::make_pair(data_gotten, std::tuple<Keys...>(keys...)));
                                error_code = SUCCESS;
                                error_message = "Successful";
                              }
                              else{
                                error_code = VALUE_ERROR;
                                error_message = "Scylla Error: Unable to get the value: " + std::string(cass_error_desc(error));
                                break; // Error: Quit out.
                              }
                            }
                            else{
                              error_code = VALUE_ERROR;
                              error_message = "Scylla Error: Unable to get the value";
                              break; // Error: Quit out.
                            }
                          }
                          else{
                            error_code = NOT_FOUND;
                            error_message = "Error: Value Not Found";
                            break; // Error: Quit out.
                          }
                        }
                        cass_iterator_free(iterator);
                      }
                      else{
                        error_code = NOT_FOUND;
                        error_message = count == 0 or count == std::tuple_size<std::tuple<Keys...>>{} ? "Error: Value Not Found" :
                                                        "Error: Value Not Found. Did you specify the entire partition key?";
                      }
                      cass_result_free(cass_result);
                      cass_future_free(result_future); // Free it here, because we break out of the for loop.
                      break; // End in "success": No need to reduce consistency.
                    }
                  }
                  cass_future_free(result_future);
                }
              }
            }
          }
          cass_statement_free(statement);
        }
      }

      return ValuStor::Result(error_code, error_message, std::move(retrieved_data));

    }

  private:
    // ****************************************************************************************************
    /// @name            store
    ///
    /// @brief           Internal version of store() used by the backlog.
    ///
    template <class Tuple, size_t... IndexSequence>
    ValuStor::Result call_store(Tuple t,
                                const Val_T& value,
                                int32_t seconds_ttl,
                                InsertMode_t insert_mode,
                                int64_t insert_microseconds_since_epoch,
                                Indices<IndexSequence...>) {
      return store( std::get<IndexSequence>(t)..., value, seconds_ttl, insert_mode, insert_microseconds_since_epoch);
    }

  public:
    // ****************************************************************************************************
    /// @name            store
    ///
    /// @brief           Get the value associated with the provided keys.
    ///
    /// @param           keys
    /// @param           value
    ///
    /// @return          'true' if successful, 'false' otherwise.
    ///
    ValuStor::Result store(const Keys&... keys,
                           const Val_T& value,
                           int32_t seconds_ttl = 0,
                           InsertMode_t insert_mode = DEFAULT_BACKLOG_MODE,
                           int64_t insert_microseconds_since_epoch = 0){

      if(insert_mode == DEFAULT_BACKLOG_MODE){
        insert_mode = this->default_backlog_mode;
      }
      ErrorCode_t error_code = UNKNOWN_ERROR;
      std::string error_message = "Scylla Error";

      if(insert_mode == USE_ONLY_BACKLOG){
        int64_t the_time = insert_microseconds_since_epoch != 0 ?
                            insert_microseconds_since_epoch :
                            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        {
          std::lock_guard<std::mutex> lock(*this->backlog_mutex_ptr);
          this->backlog_queue_ptr->push_back(std::tuple<std::tuple<Keys...>,Val_T,int32_t,int64_t>(
                                               std::tuple<Keys...>(keys...), value, seconds_ttl, the_time));
        }
        error_code = SUCCESS;
        error_message = "Backlogged";
      }
      else if(not this->is_initialized){
        error_code = SESSION_FAILED;
        error_message = "Scylla Error: Could not connect to server(s)";
      }
      else if(this->prepared_insert == nullptr){
        error_code = PREPARED_INSERT_FAILED;
        error_message = "Scylla Error: Prepared Insert Failed";
      }
      else{
        CassStatement* statement = cass_prepared_bind(this->prepared_insert);
        if(statement != nullptr){
          if(insert_microseconds_since_epoch != 0){
            cass_statement_set_timestamp(statement, insert_microseconds_since_epoch);
          } // if

          CassError error = CASS_OK;
          {
            std::pair<CassError, size_t> error_keys = ValuStor::bind(statement, (size_t)0, keys...);
            if(error_keys.first == CASS_OK){
              std::pair<CassError, size_t> error_value = ValuStor::bind(statement, error_keys.second, value);
              if(error_value.first == CASS_OK){
                std::pair<CassError, size_t> error_ttl = ValuStor::bind(statement, error_keys.second + 1, seconds_ttl);
                error = error_ttl.first;
              }
              else{
                error = error_value.first;
              }
            }
            else{
              error = error_keys.first;
            }
          }
          if(error != CASS_OK){
            error_code = BIND_ERROR;
            error_message = "Scylla Error: Unable to bind parameters: " + std::string(cass_error_desc(error));
          }
          else{
            for(const auto& level : this->write_consistencies){
              error = cass_statement_set_consistency(statement, level);
              if(error != CASS_OK){
                error_code = CONSISTENCY_ERROR;
                error_message = "Scylla Error: Unable to set statement consistency: " + std::string(cass_error_desc(error));
              }
              else{
                CassFuture* result_future = cass_session_execute(this->session, statement);
                if(result_future != nullptr){
                  cass_future_wait_timed(result_future, 2000000L); // Wait up to 2s
                  if (cass_future_error_code(result_future) != CASS_OK) {
                    error_code = QUERY_ERROR;
                    error_message = logFutureErrorMessage(result_future, "Unable to run query");
                  }
                  else{
                    error_code = SUCCESS;
                    error_message = "Value stored successfully";
                    cass_future_free(result_future);  // Free it here after a successful insertion, but prior to breaking out of the loop.
                    break;
                  }
                  cass_future_free(result_future);
                }
              }
            }
          }
          cass_statement_free(statement);
        }
      }

      if(error_code != SUCCESS and insert_mode == ALLOW_BACKLOG){
        int64_t the_time = insert_microseconds_since_epoch != 0 ?
                            insert_microseconds_since_epoch :
                            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        {
          std::lock_guard<std::mutex> lock(*this->backlog_mutex_ptr);
          this->backlog_queue_ptr->push_back(std::tuple<std::tuple<Keys...>,Val_T,int32_t,int64_t>(
                                              std::tuple<Keys...>(keys...), value, seconds_ttl, the_time));
        }
      }

      return ValuStor::Result(error_code, error_message, value, std::tuple<Keys...>(keys...));

    }

  //
  // All of the following functions are used to facilitate various template parameters.
  //
  private:
    //
    // bind() functions are used to attach templated parameters (both keys and values) to the prepared statements.
    //
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const int8_t& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_int8(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const int16_t& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_int16(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const int32_t& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_int32(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const uint32_t& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_uint32(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const int64_t& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_int64(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const float& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_float(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const double& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_double(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const std::string& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_string_n(stmt, index, value.c_str(), value.size()), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const char* value) {
      return std::pair<CassError, size_t>(cass_statement_bind_string_n(stmt, index, value, std::strlen(value)), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const cass_bool_t& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_bool(stmt, index, value), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const bool& value) {
      cass_bool_t cass_bool = value ? cass_true : cass_false;
      return std::pair<CassError, size_t>(cass_statement_bind_bool(stmt, index, cass_bool), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const CassUuid& value) {
      CassUuid uuid = value;
      if(value.time_and_version == 0 and value.clock_seq_and_node == 0){
        cass_uuid_gen_time(ValuStorUUIDGen::instance().uuid_gen, &uuid);
      } // if
      return std::pair<CassError, size_t>(cass_statement_bind_uuid(stmt, index, uuid), 1);
    }
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const std::vector<uint8_t>& value) {
      return std::pair<CassError, size_t>(cass_statement_bind_bytes(stmt, index, value.data(), value.size()), 1);
    }
    #if defined(NLOHMANN_JSON_HPP)
    static std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const nlohmann::json& value){
      std::string json_as_str = value.dump();
      return std::pair<CassError, size_t>(cass_statement_bind_string_n(stmt, index, json_as_str.c_str(), json_as_str.size()), 1);
    }
    #endif

    template<typename Value_T, typename... Vals>
    std::pair<CassError, size_t> bind(CassStatement* stmt, size_t index, const Value_T& value, const Vals&... values) {
      CassError error = bind(stmt, index, value).first;
      if(error == CASS_OK){
        std::pair<CassError, size_t> result = bind(stmt, index + 1, values...);
        result.second++;
        return result;
      }
      else{
        return std::pair<CassError, size_t>(error, 0);
      }
    }

    //
    // get() functions are used to convert data returned in a SELECT into the target format.
    //
    static CassError get(const CassValue* value, int8_t* target){
      return cass_value_get_int8(value, target);
    }
    static CassError get(const CassValue* value, int16_t* target){
      return cass_value_get_int16(value, target);
    }
    static CassError get(const CassValue* value, int32_t* target){
      return cass_value_get_int32(value, target);
    }
    static CassError get(const CassValue* value, uint32_t* target){
      return cass_value_get_uint32(value, target);
    }
    static CassError get(const CassValue* value, int64_t* target){
      return cass_value_get_int64(value, target);
    }
    static CassError get(const CassValue* value, float* target){
      return cass_value_get_float(value, target);
    }
    static CassError get(const CassValue* value, double* target){
      return cass_value_get_double(value, target);
    }
    static CassError get(const CassValue* value, std::string* target){
      const char* str;
      size_t str_length;
      CassError error = cass_value_get_string(value, &str, &str_length);
      *target = std::string(str, str + str_length);
      return error;
    }
    static CassError get(const CassValue* value, cass_bool_t* target){
      return cass_value_get_bool(value, target);
    }
    static CassError get(const CassValue* value, bool* target){
      cass_bool_t cass_bool = cass_false;
      auto result = cass_value_get_bool(value, &cass_bool);
      *target = cass_bool == cass_true;
      return result;
    }
    static CassError get(const CassValue* value, CassUuid* target){
      return cass_value_get_uuid(value, target);
    }
    static CassError get(const CassValue* value, std::vector<uint8_t>* target){
      const cass_byte_t* cass_bytes;
      size_t array_length;
      CassError error = cass_value_get_bytes(value, &cass_bytes, &array_length);
      *target = std::vector<uint8_t>(cass_bytes, cass_bytes + array_length);
      return error;
    }
    #if defined(NLOHMANN_JSON_HPP)
    static CassError get(const CassValue* value, nlohmann::json* target){
      std::string json_as_str;
      auto result = get(value, &json_as_str);
      *target = nlohmann::json::parse(json_as_str);
      return result;
    }
    #endif

    template<typename Value_T>
    CassError get(const CassRow* row, const size_t column, const size_t count, Value_T* value){
      CassError error = CASS_OK;
      if(count == 0){
        error = CASS_OK;
      }
      else{
        if(column > count){
          const CassValue* cass_value = cass_row_get_column(row, column - count);
          if(cass_value != nullptr){
            error = get(cass_value, value);
          }
        }
      }
      return error;
    }

    template<typename Value_T, typename... Vals>
    CassError get(const CassRow* row, const size_t column, const size_t count, Value_T* value, Vals*... values) {
      CassError error = CASS_OK;
      if(count == 0){
        error = CASS_OK;
      }
      else{
        if(column > count){
          const CassValue* cass_value = cass_row_get_column(row, column - count);
          if(cass_value != nullptr){
            error = get(cass_value, value);
          }
        }
        if(error == CASS_OK){
          error = get(row, column + 1, count, values...);
        }
      }
      return error;
    }

  private:
    static std::string convertToStr(const int8_t& value)  {  return std::to_string(value); }
    static std::string convertToStr(const int16_t& value) {  return std::to_string(value); }
    static std::string convertToStr(const int32_t& value) {  return std::to_string(value); }
    static std::string convertToStr(const uint32_t& value) { return std::to_string(value); }
    static std::string convertToStr(const int64_t& value) {  return std::to_string(value); }
    static std::string convertToStr(const float& value) {    return std::to_string(value); }
    static std::string convertToStr(const double& value) {   return std::to_string(value); }
    static std::string convertToStr(const std::string& value) { return value; }
    static std::string convertToStr(const char* value) {     return std::string(value);    }
    static std::string convertToStr(const cass_bool_t& value) { return std::to_string(value == cass_true); }
    static std::string convertToStr(const bool& value) {     return std::to_string(value); }
    static std::string convertToStr(const CassUuid& value) {
      char* output = nullptr;
      cass_uuid_string(value, output);
      return output != nullptr ? std::string(output) : "";
    }
    static std::string convertToStr(const std::vector<uint8_t>& value) { return std::string(value); }
    #if defined(NLOHMANN_JSON_HPP)
    static std::string convertToStr(const nlohmann::json& value){ return value.dump(); }
    #endif

    static void convertFromStr(const std::string& source, int8_t* dest)   { *dest = std::atoll(source.c_str());  }
    static void convertFromStr(const std::string& source, int16_t* dest)  { *dest = std::atoll(source.c_str());  }
    static void convertFromStr(const std::string& source, int32_t* dest)  { *dest = std::atoll(source.c_str());  }
    static void convertFromStr(const std::string& source, uint32_t* dest) { *dest = std::atoll(source.c_str());  }
    static void convertFromStr(const std::string& source, int64_t* dest)  { *dest = std::atoll(source.c_str());  }
    static void convertFromStr(const std::string& source, cass_bool_t* dest)  {
      *dest = (source != "" and source != "0" and source != "false" and source != "False") ? cass_true : cass_false;
    }
    static void convertFromStr(const std::string& source, bool* dest)  {
      *dest = (source != "" and source != "0" and source != "false" and source != "False") ? true : false;
    }
    static void convertFromStr(const std::string& source, float* dest)  {  *dest = std::atof(source.c_str()); }
    static void convertFromStr(const std::string& source, double* dest)  { *dest = std::atof(source.c_str()); }
    static void convertFromStr(const std::string& source, std::string* dest)  {
      *dest = source;
    }
    static void convertFromStr(const std::string& source, CassUuid* dest)  {
      cass_uuid_from_string(source.c_str(), dest);
    }
    static void convertFromStr(const std::string& source, std::vector<uint8_t>* dest)  {
      *dest = source;
    }
    #if defined(NLOHMANN_JSON_HPP)
    static void convertFromStr(const std::string& source, nlohmann::json* dest)  {
      *dest = nlohmann::json::parse(source);
    }
    #endif

  public:
    // ****************************************************************************************************
    /// @name            valueToString
    ///
    /// @brief           Convert a Val_T value to a string.
    ///                  For example:
    ///                    {
    ///                      std::string key = "0";
    ///                      auto result = ValuStor::retrieve(ValuStor::stringToKey(key));
    ///                      return ValuStor::valueToString(result.data);
    ///                    }
    ///
    std::string valueToString(const Val_T& value){ return convertToStr(value); }

    // ****************************************************************************************************
    /// @name            stringToValue
    ///
    /// @brief           Convert a string into a Val_T
    ///                  For example:
    ///                    {
    ///                      std::string key = "0";
    ///                      std::string value = "0";
    ///                      ValuStor::store( ValuStor::stringToKey(key),
    ///                                       ValuStor::stringToValue(value) )
    ///                    }
    ///
    Val_T stringToValue(std::string value){ Val_T v{}; convertFromStr(value, &v); return v; }

    // ****************************************************************************************************
    /// @name            stringToKey
    ///
    /// @brief           Convert a string into the first type in the template "Keys..."
    ///                  For example:
    ///                    {
    ///                      std::string key = "0";
    ///                      std::string value = "0";
    ///                      ValuStor::store( ValuStor::stringToKey(key),
    ///                                       ValuStor::stringToValue(value) )
    ///                    }
    ///
    std::tuple<Keys...> stringToKey(std::string key){ std::tuple<Keys...> values{}; convertFromStr(key, &std::get<0>(values)); return values; }

};

} // end namespace ValuStor

#endif /* #ifndef VALUE_STORE_H */

