/*****************************************************************************\
*  Copyright (c) 2023 Ricardo Machado, Sydney, Australia All rights reserved.
*
*  MIT License
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to
*  deal in the Software without restriction, including without limitation the
*  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
*  sell copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
*  IN THE SOFTWARE.
*
*  You should have received a copy of the MIT License along with this program.
*  If not, see https://opensource.org/licenses/MIT.
\*****************************************************************************/
#pragma once

#include <lmdb.h>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <cstring>

#ifndef S_IRUSR
   #define S_IRUSR   00400    // read by owner
#endif

#ifndef S_IWUSR
   #define S_IWUSR   00200    // write by owner
#endif

namespace lmdb {

   constexpr unsigned int DEFAULT_MAXTABLES = 128;
   constexpr unsigned int DEFAULT_MAXREADERS = 126;
   constexpr size_t DEFAULT_MMAPSIZE = 10485760;

   enum class transaction_type_t { read_write, read_only };

   constexpr int MDB_TABLE_ALREADY_OPEN = MDB_LAST_ERRCODE + 1;
   constexpr int MDB_TABLE_NOT_OPEN = MDB_LAST_ERRCODE + 2;
   constexpr int MDB_TRANSACTION_HANDLE_NULL = MDB_LAST_ERRCODE + 3;
   constexpr int MDB_TRANSACTION_ALREADY_STARTED = MDB_LAST_ERRCODE + 4;

   class status_t
   {
      int error_{ MDB_SUCCESS };

   public:
      status_t() = default;
      status_t(const status_t&) = default;
      status_t(status_t&&) noexcept = default;
      status_t& operator=(const status_t&) = default;
      status_t& operator=(status_t&&) = default;

      explicit status_t(int retcode) noexcept
         : error_{ retcode }
      {}

      status_t& operator=(int retcode) noexcept
      {
         error_ = retcode;
         return *this;
      }

      bool ok() const noexcept { return error_ == MDB_SUCCESS; }
      bool nok() const noexcept { return error_ != MDB_SUCCESS; }
      int error() const noexcept { return error_; }

      std::string message() const noexcept
      {
         switch (error_)
         {
         case MDB_SUCCESS: return "Success";
         case MDB_TABLE_ALREADY_OPEN: return "Table already open";
         case MDB_TABLE_NOT_OPEN: return "Table not open";
         case MDB_TRANSACTION_HANDLE_NULL: return "Transaction handle not initialized";
         case MDB_TRANSACTION_ALREADY_STARTED: return "Transaction already started";
         }
         return mdb_strerror(error_);
      }
   };

   class environment_t
   {
      MDB_env* envptr_{ nullptr };
      size_t max_tables_{ 0 };
      size_t mmap_size_{ 0 };

   public:
      environment_t() = default;
      environment_t(const environment_t&) = delete;
      environment_t& operator=(const environment_t&) = delete;

      ~environment_t()
      {
         cleanup();
      }

      environment_t(environment_t&& other) noexcept
         : envptr_{ other.envptr_ }
         , max_tables_{ other.max_tables_ }
         , mmap_size_{ other.mmap_size_ }
      {
         other.envptr_ = nullptr;
         other.max_tables_ = 0;
         other.mmap_size_ = 0;
      }

      environment_t& operator=(environment_t&& other) noexcept
      {
         if (this != &other)
         {
            envptr_ = other.envptr_;
            other.envptr_ = nullptr;
            max_tables_ = other.max_tables_;
            other.max_tables_ = 0;
            mmap_size_ = other.mmap_size_;
            other.mmap_size_ = 0;
         }
         return *this;
      }

      status_t startup(const std::string& path, unsigned int max_tables = DEFAULT_MAXTABLES, size_t mmap_size = DEFAULT_MMAPSIZE, unsigned int max_readers = DEFAULT_MAXREADERS) noexcept
      {
         status_t status;
         if (status = mdb_env_create(&envptr_); status.nok()) return status;
         if (status = mdb_env_set_maxdbs(envptr_, max_tables); status.nok()) return status;
         if (status = mdb_env_set_mapsize(envptr_, mmap_size); status.nok()) return status;
         if (status = mdb_env_set_maxreaders(envptr_, max_readers); status.nok()) return status;
         if (status = mdb_env_open(envptr_, path.c_str(), 0, (S_IRUSR | S_IWUSR)); status.nok()) return status;
         max_tables_ = max_tables;
         mmap_size_ = mmap_size;
         return status;
      }

      void cleanup() noexcept
      {
         if (envptr_)
         {
            mdb_env_close(envptr_);
            envptr_ = nullptr;
         }
      }

      // return the number of stale slots cleared
      int check() noexcept
      {
         int dead{ 0 };
         if (!envptr_ && (mdb_reader_check(envptr_, &dead) >= 0)) return dead;
         return 0;
      }

      // flush buffes to disk
      status_t flush() noexcept
      {
         return status_t(mdb_env_sync(envptr_, 0));
      }

      std::string path() const noexcept
      {
         const char* ptr{ nullptr };
         if (envptr_)
         {
            int rc = mdb_env_get_path(envptr_, &ptr);
            if (rc != MDB_SUCCESS) return std::string();
         }
         return std::string(ptr ? ptr : "");
      }

      size_t max_readers() const noexcept
      {
         unsigned int readers{ 0 };
         if (int rc = mdb_env_get_maxreaders(envptr_, &readers); rc != MDB_SUCCESS) return 0;
         return readers;
      }

      size_t max_tables() const noexcept
      {
         return max_tables_;
      }

      size_t mmap_size() const noexcept
      {
         return mmap_size_;
      }

      size_t mmap_size(size_t sz) noexcept
      {
         if (status = mdb_env_set_mapsize(envptr_, sz); status.nok()) return status;
      }

      size_t max_keysize() const noexcept
      {
         int count{ 0 };
         if (envptr_)
         {
            count = mdb_env_get_maxkeysize(envptr_);
         }
         return size_t(count);
      }

      MDB_env* handle() noexcept
      {
         return envptr_;
      }
   }; // class environment_t

   class transaction_t
   {
      environment_t& env_;
      MDB_txn* txnptr_{ nullptr };

   public:
      transaction_t() = delete;
      transaction_t(const transaction_t&) = delete;
      transaction_t& operator=(const transaction_t&) = delete;

      explicit transaction_t(environment_t& env)
         : env_{ env }
      {}

      ~transaction_t() noexcept
      {
         abort();
      }

      transaction_t(transaction_t&& other) noexcept
         : env_{ other.env_ }
         , txnptr_{ other.txnptr_ }
      {
         other.txnptr_ = nullptr;
      }

      transaction_t& operator=(transaction_t&& other) noexcept
      {
         if (this != &other)
         {
            txnptr_ = other.txnptr_;
            other.txnptr_ = nullptr;
         }
         return *this;
      }

      status_t begin(transaction_type_t type) noexcept
      {
         if (txnptr_)
         {
            if (status_t status(mdb_txn_commit(txnptr_));  status.nok()) return status;
         }
         if (int rc = mdb_txn_begin(env_.handle(), nullptr, get_type(type), &txnptr_); rc != MDB_SUCCESS)
         {
            return status_t(rc);
         }
         return status_t();
      }

      status_t commit() noexcept
      {
         if (!txnptr_) return status_t(MDB_TRANSACTION_HANDLE_NULL);
         if (int rc = mdb_txn_commit(txnptr_); rc != MDB_SUCCESS)
         {
            return status_t(rc);
         }
         txnptr_ = nullptr;
         return status_t();
      }

      status_t abort() noexcept
      {
         if (!txnptr_) return status_t(MDB_TRANSACTION_HANDLE_NULL);
         mdb_txn_abort(txnptr_);
         txnptr_ = nullptr;
         return status_t();
      }

      bool started() const noexcept
      {
         return txnptr_ != nullptr;
      }

      MDB_txn* handle() noexcept
      {
         return txnptr_;
      }

      environment_t& environment() noexcept
      {
         return env_;
      }

   private:
      int get_type(transaction_type_t type) const noexcept
      {
         return (type == transaction_type_t::read_write) ? 0 : MDB_RDONLY;
      }
   }; // class transaction_t

   template <typename T>
   class data_t
   {
      MDB_val data_{};

   public:
      using value_type = typename T::value_type;
      using reference = value_type&;
      using const_reference = const value_type&;
      using pointer = value_type*;
      using const_pointer = const value_type*;

      data_t(const data_t&) = default;
      data_t(data_t&&) noexcept = default;
      data_t& operator=(const data_t&) = default;
      data_t& operator=(data_t&&) noexcept = default;

      data_t() noexcept
      {
         data_.mv_size = 0;
         data_.mv_data = nullptr;
      }

      explicit data_t(const T& obj) noexcept
      {
         data_.mv_data = nullptr;
         data_.mv_size = obj.size();
         if (data_.mv_size > 0)
         {
            data_.mv_data = (void*)(&obj[0]);
         }
      }

      size_t size() const noexcept
      {
         return data_.mv_size;
      }

      const MDB_val* data() const noexcept
      {
         return &data_;
      }

      MDB_val* data() noexcept
      {
         return &data_;
      }

      void get(T& obj) noexcept
      {
         if (data_.mv_size > 0)
         {
            obj.resize(data_.mv_size);
            std::memcpy((void*)&obj[0], data_.mv_data, data_.mv_size);
            return;
         }
         obj.clear();
      }

      void set(T& obj) noexcept
      {
         data_.mv_size = obj.size();
         data_.mv_data = nullptr;
         if (data_.mv_size > 0)
         {
            data_.mv_data = &obj[0];
         }
      }

      void set(const T& obj) noexcept
      {
         data_.mv_size = obj.size();
         data_.mv_data = nullptr;
         if (data_.mv_size > 0)
         {
            data_.mv_data = (void*)&obj[0];
         }
      }
   };

   template <typename KEY, typename VALUE>
   class table_base_t
   {
      environment_t& env_;
      MDB_dbi id_{ 0 };
      MDB_txn* txnptr_{ nullptr };
      bool opened_{ false };

   public:
      using key_type = KEY;
      using key_reference = KEY&;
      using key_const_reference = const KEY&;
      using value_type = VALUE;
      using value_reference = VALUE&;
      using value_const_reference = const VALUE&;
      using key_value_t = std::pair<KEY, VALUE>;

      table_base_t() = delete;
      table_base_t(const table_base_t&) = delete;
      table_base_t& operator=(const table_base_t&) = delete;

      explicit table_base_t(environment_t& env) noexcept
         : env_{ env }
      {}

      ~table_base_t() noexcept
      {
         close();
      }

      table_base_t(table_base_t&& other) noexcept
         : env_{ other.env_ }
         , id_{ other.id_ }
         , txnptr_ { other.txnptr_ }
         , opened_{ other.opened_ }
      {
         other.id_ = 0;
         other.txnptr_ = nullptr;
         other.opened_ = false;
      }

      table_base_t& operator=(table_base_t&& other) noexcept
      {
         if (this != &other)
         {
            id_ = other.id_;
            opened_ = other.opened_;
            txnptr_ = other.txnptr_;
            other.id_ = 0;
            other.opened_ = false;
            other.txnptr_ = nullptr;
         }
         return *this;
      }

      status_t create(transaction_t& txn, const std::string& path) noexcept
      {
         txnptr_ = txn.handle();
         return open(path, true);
      }

      status_t open(transaction_t& txn, const std::string& path) noexcept
      {
         txnptr_ = txn->handle();
         return open(path, false);
      }

      status_t close() noexcept
      {
         if (!opened_) return status_t(MDB_TABLE_NOT_OPEN);
         opened_ = false;
         txnptr_ = nullptr;
         return status_t();
      }

      status_t drop() noexcept
      {
         status_t status{ MDB_TABLE_NOT_OPEN };
         if (!opened_) return status;
         if (status = mdb_drop(txnptr_, id_, 1); status.nok()) return status;
         opened_ = false;
         return status;
      }

      status_t get(const key_const_reference target_key, key_reference key, value_reference value) noexcept
      {
         status_t status{ MDB_TABLE_NOT_OPEN };
         if (!opened_) return status;
         data_t<std::string> k(target_key), v;
         if (status = mdb_get(txnptr_, id_, k.data(), v.data()); status.nok()) return status;
         k.get(key);
         v.get(value);
         return status;
      }

      status_t get(const key_const_reference target_key, key_value_t& kv) noexcept
      {
         return get(target_key, kv.first, kv.second);
      }

      status_t put(key_const_reference key, value_const_reference value) noexcept
      {
         if (!opened_) return status_t(MDB_TABLE_NOT_OPEN);
         data_t k(key);
         data_t v(value);
         return status_t(mdb_put(txnptr_, id_, k.data(), v.data(), 0));
      }

      status_t put(const key_value_t& kv) noexcept
      {
         return put(kv.first, kv.second);
      }

      status_t del(key_const_reference key, value_const_reference value) noexcept
      {
         if (!opened_) return status_t(MDB_TABLE_NOT_OPEN);
         data_t k(key);
         data_t v(value);
         return status_t(mdb_del(txnptr_, id_, k.data(), v.data()));
      }

      status_t del(const key_value_t& kv) noexcept
      {
         return del(kv.first, kv.second);
      }

      MDB_dbi handle() const noexcept
      {
         return id_;
      }

      environment_t& environment() noexcept
      {
         return env_;
      }

   private:
      status_t open(const std::string& path, bool create) noexcept
      {
         status_t status;
         if (opened_) return status_t(MDB_TABLE_ALREADY_OPEN);
         if (status = mdb_dbi_open(txnptr_, path.c_str(), (create ? MDB_CREATE : 0), &id_); status.nok()) return status;
         opened_ = true;
         return status;
      }
   }; // class table_base_t

   class cursor_exception_t : public std::runtime_error
   {
      int errcode_{ 0 };

   public:
      cursor_exception_t(status_t status)
         : errcode_{ status.error() }
         , runtime_error(status.message().c_str())
      {}

      int error() const
      {
         return errcode_;
      }
   };

   template <typename KEY, typename VALUE>
   class cursor_base_t
   {
      MDB_cursor* cursor_{ nullptr };

   public:
      using key_type = KEY;
      using key_reference = KEY&;
      using key_const_reference = const KEY&;
      using value_type = VALUE;
      using value_reference = VALUE&;
      using value_const_reference = const VALUE&;
      using key_value_t = std::pair<KEY, VALUE>;

      cursor_base_t() = delete;
      cursor_base_t(const cursor_base_t&) = delete;
      cursor_base_t& operator=(const cursor_base_t&) = delete;

      ~cursor_base_t() noexcept
      {
         if (cursor_)
         {
            mdb_cursor_close(cursor_);
            cursor_ = nullptr;
         }
      }

      explicit cursor_base_t(transaction_t& txn, table_base_t<KEY, VALUE>& table)
      {
         if (int rc = mdb_cursor_open(txn.handle(), table.handle(), &cursor_); rc != MDB_SUCCESS)
         {
            throw cursor_exception_t(status_t(rc));
         }
      }

      cursor_base_t(cursor_base_t&& other) noexcept
         : cursor_{ other.cursor_ }
      {
         other.cursor_ = nullptr;
      }

      cursor_base_t& operator=(cursor_base_t&& other) noexcept
      {
         if (this != &other)
         {
            cursor_ = other.cursor_;
            other.cursor_ = nullptr;
         }
         return *this;
      }

      status_t current(key_reference key, value_reference value) noexcept
      {
         return get(key, value, MDB_GET_CURRENT);
      }

      status_t current(key_reference key) noexcept
      {
         return get(key, MDB_GET_CURRENT);
      }

      status_t first(key_reference key, value_reference value) noexcept
      {
         return get(key, value, MDB_FIRST);
      }

      status_t first(key_value_t& kv) noexcept
      {
         return first(kv.first, kv.second);
      }

      status_t first(key_reference key) noexcept
      {
         return get(key, MDB_FIRST);
      }

      status_t first() noexcept
      {
         key_type k;
         return first(k);
      }

      status_t last(key_reference key, value_reference value) noexcept
      {
         return get(key, value, MDB_LAST);
      }

      status_t last(key_value_t& kv) noexcept
      {
         return last(kv.first, kv.second);
      }

      status_t last(key_reference key) noexcept
      {
         return get(key, MDB_LAST);
      }

      status_t last() noexcept
      {
         key_type k;
         return last(k);
      }

      status_t next(key_reference key, value_reference value) noexcept
      {
         return get(key, value, MDB_NEXT);
      }

      status_t next(key_value_t& kv) noexcept
      {
         return next(kv.first, kv.second);
      }

      status_t next(key_reference key) noexcept
      {
         return get(key, MDB_NEXT);
      }

      status_t next() noexcept
      {
         key_type k;
         return next(k);
      }

      status_t prior(key_reference key, value_reference value) noexcept
      {
         return get(key, value, MDB_PREV);
      }

      status_t prior(key_value_t& kv) noexcept
      {
         return prior(kv.first, kv.second);
      }

      status_t prior(key_reference key) noexcept
      {
         return get(key, MDB_PREV);
      }

      status_t prior() noexcept
      {
         key_type k;
         return prior(k);
      }

      status_t seek(key_const_reference target_key) noexcept
      {
         key_type k{ target_key };
         value_type v;
         if (status_t status = get(k, v, MDB_SET); status.nok()) return status;
         return status_t();
      }

      status_t find(key_const_reference& target_key, key_reference key, value_reference value) noexcept
      {
         key_type k{ target_key };
         if (status_t status = get(k, value, MDB_SET_KEY); status.nok()) return status;
         key = k;
         return status_t();
      }

      status_t search(key_const_reference& target_key, key_reference key, value_reference value) noexcept
      {
         key_type k{ target_key };
         if (status_t status = get(k, value, MDB_SET_RANGE); status.nok()) return status;
         key = k;
         return status_t();
      }

      status_t put(key_const_reference key, value_const_reference value) noexcept
      {
         data_t<KEY> k(key);
         data_t<VALUE> v(value);
         return status_t(mdb_cursor_put(cursor_, k.data(), v.data(), 0));
      }

      status_t del() noexcept
      {
         return status_t(mdb_cursor_del(cursor_, 0));
      }

   private:
      status_t get(key_reference key, value_reference value, MDB_cursor_op op) noexcept
      {
         status_t status;
         data_t<KEY> k;
         data_t<VALUE> v;
         if (is_set_operation(op)) k.set(key);
         if (status = mdb_cursor_get(cursor_, k.data(), v.data(), op); status.nok()) return status;
         if (op != MDB_SET)
         {
            k.get(key);
            v.get(value);
         }
         return status;
      }

      status_t get(key_reference key, MDB_cursor_op op) noexcept
      {
         status_t status;
         data_t<KEY> k;
         data_t<VALUE> v;
         if (is_set_operation(op)) k.set(key);
         if (status = mdb_cursor_get(cursor_, k.data(), v.data(), op); status.nok()) return status;
         if (op != MDB_SET) k.get(key);
         return status;
      }

      bool is_set_operation(MDB_cursor_op op) noexcept
      {
         return (op == MDB_SET || op == MDB_SET_KEY || op == MDB_SET_RANGE);
      }
   }; // class cursor_base_t

   using table_t = table_base_t<std::string, std::string>;
   using cursor_t = cursor_base_t<std::string, std::string>;

} // namespace lmdb
