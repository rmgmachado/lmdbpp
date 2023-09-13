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
#include <string_view>
#include <utility>
#include <stdexcept>
#include <cstring>

namespace lmdb {
   
   constexpr int DEFAULT_MODE = 00644;
   constexpr unsigned int DEFAULT_MAXSTORES = 128;
   constexpr unsigned int DEFAULT_MAXREADERS = 126;
   constexpr size_t DEFAULT_MMAPSIZE = 10485760;

   enum class transaction_type_t { read_write, read_only, none };

   constexpr int MDB_ALREADY_OPEN = MDB_LAST_ERRCODE + 1;
   constexpr int MDB_NOT_OPEN = MDB_LAST_ERRCODE + 2;
   constexpr int MDB_TRANSACTION_HANDLE_NULL = MDB_LAST_ERRCODE + 3;
   constexpr int MDB_TRANSACTION_ALREADY_STARTED = MDB_LAST_ERRCODE + 4;
   constexpr int MDB_INVALID_TRANSACTION_TYPE = MDB_LAST_ERRCODE + 5;

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

      bool operator==(const status_t& other) const noexcept
      {
         return error_ == other.error_;
      }

      bool operator!=(const status_t& other) const noexcept

      {
         return error_ != other.error_;
      }

      bool ok() const noexcept { return error_ == MDB_SUCCESS; }
      bool nok() const noexcept { return error_ != MDB_SUCCESS; }
      int error() const noexcept { return error_; }

      std::string message() const noexcept
      {
         switch (error_)
         {
         case MDB_SUCCESS: return "Success";
         case MDB_ALREADY_OPEN: return "Table or cursor already open";
         case MDB_NOT_OPEN: return "Table or cursor not open";
         case MDB_TRANSACTION_HANDLE_NULL: return "Transaction handle not initialized";
         case MDB_TRANSACTION_ALREADY_STARTED: return "Transaction already started";
         case MDB_INVALID_TRANSACTION_TYPE: return "Invalid transaction type";
         }
         return mdb_strerror(error_);
      }
   };

   class error_t : public std::runtime_error
   {
      status_t status_;

   public:
      error_t() = delete;
      error_t(const status_t& status)
         : runtime_error(status.message().c_str())
      {}

      int error() const { return status_.error(); }
      status_t status() const { return status_; }
   };

   class database_t
   {
      MDB_env* envptr_{ nullptr };
      size_t max_store_{ 0 };
      size_t mmap_size_{ 0 };

   public:
      database_t() = default;
      database_t(const database_t&) = delete;
      database_t& operator=(const database_t&) = delete;

      ~database_t()
      {
         cleanup();
      }

      database_t(const std::string& path, unsigned int max_tables = DEFAULT_MAXSTORES, size_t mmap_size = DEFAULT_MMAPSIZE, unsigned int max_readers = DEFAULT_MAXREADERS, int mode = DEFAULT_MODE)
      {
         if (status_t status = initialize(path, max_tables, mmap_size, max_readers, mode); status.nok())
         {
            throw error_t(status);
         }
      }

      database_t(database_t&& other) noexcept
         : envptr_{ other.envptr_ }
         , max_store_{ other.max_store_ }
         , mmap_size_{ other.mmap_size_ }
      {
         other.envptr_ = nullptr;
         other.max_store_ = 0;
         other.mmap_size_ = 0;
      }

      database_t& operator=(database_t&& other) noexcept
      {
         if (this != &other)
         {
            envptr_ = other.envptr_;
            other.envptr_ = nullptr;
            max_store_ = other.max_store_;
            other.max_store_ = 0;
            mmap_size_ = other.mmap_size_;
            other.mmap_size_ = 0;
         }
         return *this;
      }

      status_t initialize(const std::string& path, unsigned int max_stores = DEFAULT_MAXSTORES, size_t mmap_size = DEFAULT_MMAPSIZE, unsigned int max_readers = DEFAULT_MAXREADERS, int mode = DEFAULT_MODE) noexcept
      {
         status_t status;
         if (status = mdb_env_create(&envptr_); status.nok())
         {
            return status;
         }
         if (status = mdb_env_set_maxdbs(envptr_, max_stores); status.nok())
         {
            return status;
         }
         if (status = mdb_env_set_mapsize(envptr_, mmap_size); status.nok())
         {
            return status;
         }
         if (status = mdb_env_set_maxreaders(envptr_, max_readers); status.nok())
         {
            return status;
         }
         if (status = mdb_env_open(envptr_, path.c_str(), 0, mode); status.nok())
         {
            return status;
         }
         max_store_ = max_stores;
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
         if (!envptr_ && (mdb_reader_check(envptr_, &dead) >= 0))
         {
            return dead;
         }
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
            if (rc != MDB_SUCCESS)
            {
               return std::string();
            }
         }
         return std::string(ptr ? ptr : "");
      }

      size_t max_readers() const noexcept
      {
         unsigned int readers{ 0 };
         if (int rc = mdb_env_get_maxreaders(envptr_, &readers); rc != MDB_SUCCESS)
         {
            return 0;
         }
         return readers;
      }

      size_t max_stores() const noexcept
      {
         return max_store_;
      }

      size_t mmap_size() const noexcept
      {
         return mmap_size_;
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
      database_t& env_;
      MDB_txn* txnptr_{ nullptr };
      transaction_type_t type_{ transaction_type_t::none };

   public:
      transaction_t() = delete;
      transaction_t(const transaction_t&) = delete;
      transaction_t& operator=(const transaction_t&) = delete;

      explicit transaction_t(database_t& env) noexcept
         : env_{ env }
      {}

      transaction_t(database_t& env, transaction_type_t type)
         : env_{ env }
      {
         if (status_t status = begin(type); status.nok())
         {
            throw error_t(status);
         }
      }

      ~transaction_t() noexcept
      {
         abort();
      }

      transaction_t(transaction_t&& other) noexcept
         : env_{ other.env_ }
         , txnptr_{ other.txnptr_ }
         , type_{ other.type_ }
      {
         other.txnptr_ = nullptr;
         other.type_ = transaction_type_t::none;
      }

      transaction_t& operator=(transaction_t&& other) noexcept
      {
         if (this != &other)
         {
            txnptr_ = other.txnptr_;
            other.txnptr_ = nullptr;
            type_ = other.type_;
            other.type_ = transaction_type_t::none;
         }
         return *this;
      }

      status_t begin(transaction_type_t type) noexcept
      {
         if (type == transaction_type_t::none)
         {
            return status_t(MDB_INVALID_TRANSACTION_TYPE);
         }
         if (txnptr_)
         {
            if (status_t status(mdb_txn_commit(txnptr_));  status.nok())
            {
                return status;
            }
            type_ = transaction_type_t::none;
         }
         if (int rc = mdb_txn_begin(env_.handle(), nullptr, get_type(type), &txnptr_); rc != MDB_SUCCESS)
         {
            return status_t(rc);
         }
         type_ = type;
         return status_t();
      }

      status_t commit() noexcept
      {
         if (!txnptr_)
         {
            return status_t(MDB_TRANSACTION_HANDLE_NULL);
         }
         if (int rc = mdb_txn_commit(txnptr_); rc != MDB_SUCCESS)
         {
            return status_t(rc);
         }
         txnptr_ = nullptr;
         type_ = transaction_type_t::none;
         return status_t();
      }

      status_t abort() noexcept
      {
         if (!txnptr_)
         {
            return status_t(MDB_TRANSACTION_HANDLE_NULL);
         }
         mdb_txn_abort(txnptr_);
         txnptr_ = nullptr;
         type_ = transaction_type_t::none;
         return status_t();
      }

      bool started() const noexcept
      {
         return txnptr_ != nullptr;
      }

      transaction_type_t type() const noexcept
      {
         return type_;
      }

      MDB_txn* handle() noexcept
      {
         return txnptr_;
      }

      database_t& database() noexcept
      {
         return env_;
      }

   private:
      int get_type(transaction_type_t type) const noexcept
      {
         switch (type)
         {
         case transaction_type_t::none: return MDB_RDONLY;
         case transaction_type_t::read_write: return 0;
         case transaction_type_t::read_only: return MDB_RDONLY;
         }
         return 0;
      }
   }; // class transaction_t

   class data_t
   {
      MDB_val data_{};

   public:
      using value_type = std::string;
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

      template <typename T>
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

      void get(std::string& str) noexcept
      {
         if (data_.mv_size > 0)
         {
            str.resize(data_.mv_size);
            std::memcpy((void*)&str[0], data_.mv_data, data_.mv_size);
            return;
         }
         str.clear();
      }

      void get(std::string_view& sv) noexcept
      {
         if (data_.mv_size > 0)
         {
            sv = std::move(std::string_view((std::string_view::pointer)data_.mv_data, data_.mv_size));
         }
         sv = std::string_view();
      }

      template <typename T>
      void set(T& obj) noexcept
      {
         data_.mv_size = obj.size();
         data_.mv_data = nullptr;
         if (data_.mv_size > 0)
         {
            data_.mv_data = &obj[0];
         }
      }

      template <typename T>
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

   class store_t
   {
      database_t& env_;
      MDB_dbi id_{ 0 };
      bool opened_{ false };
      std::string name_;

   public:
      store_t() = delete;
      store_t(const store_t&) = delete;
      store_t& operator=(const store_t&) = delete;

      explicit store_t(database_t& env) noexcept
         : env_{ env }
      {}

      ~store_t() noexcept
      {
         close();
      }

      store_t(store_t&& other) noexcept
         : env_{ other.env_ }
         , id_{ other.id_ }
         , opened_{ other.opened_ }
         , name_{ std::move(other.name_) }
      {
         other.id_ = 0;
         other.opened_ = false;
      }

      store_t& operator=(store_t&& other) noexcept
      {
         if (this != &other)
         {
            id_ = other.id_;
            opened_ = other.opened_;
            name_ = std::move(other.name_);
            other.id_ = 0;
            other.opened_ = false;
         }
         return *this;
      }

      status_t create(transaction_t& txn, const std::string& name) noexcept
      {
         return open_or_create(txn, name, true);
      }

      status_t open(transaction_t& txn, const std::string& name) noexcept
      {
         return open_or_create(txn, name, false);
      }

      status_t close(transaction_t&) noexcept
      {
         return close();
      }

      status_t drop(transaction_t& txn) noexcept
      {
         status_t status{ MDB_NOT_OPEN };
         if (!opened_)
         {
            return status;
         }
         if (status = mdb_drop(txn.handle(), id_, 1); status.nok())
         {
            return status;
         }
         opened_ = false;
         name_.clear();
         return status;
      }

      status_t get(transaction_t& txn, const std::string_view& target_key, std::string& key, std::string& value) noexcept
      {
         status_t status{ MDB_NOT_OPEN };
         if (!opened_)
         {
            return status;
         }
         data_t k(target_key), v;
         if (status = mdb_get(txn.handle(), id_, k.data(), v.data()); status.nok())
         {
            return status;
         }
         k.get(key);
         v.get(value);
         return status;
      }

      status_t put(transaction_t& txn, const std::string_view& key, const std::string_view& value) noexcept
      {
         if (!opened_)
         {
            return status_t(MDB_NOT_OPEN);
         }
         data_t k(key);
         data_t v(value);
         return status_t(mdb_put(txn.handle(), id_, k.data(), v.data(), 0));
      }

      status_t del(transaction_t& txn, const std::string_view& key, const std::string_view& value) noexcept
      {
         if (!opened_)
         {
            return status_t(MDB_NOT_OPEN);
         }
         data_t k(key);
         data_t v(value);
         return status_t(mdb_del(txn.handle(), id_, k.data(), v.data()));
      }

      size_t entries(transaction_t& txn) noexcept
      {
         MDB_stat stat;
         status_t status;
         size_t retval{ 0 };
         if (status = mdb_stat(txn.handle(), id_, &stat); status.ok())
         {
            retval = stat.ms_entries;
         }
         return retval;
      }

      std::string name() const noexcept
      {
         return name_;
      }

      MDB_dbi handle() const noexcept
      {
         return id_;
      }

      database_t& database() noexcept
      {
         return env_;
      }

   private:
      status_t open_or_create(transaction_t& txn, const std::string& name, bool create) noexcept
      {
         status_t status;
         if (opened_)
         {
            return status_t(MDB_ALREADY_OPEN);
         }
         if (status = mdb_dbi_open(txn.handle(), name.c_str(), make_mode(create), &id_); status.nok())
         {
            return status;
         }
         opened_ = true;
         return status;
      }

      status_t close() noexcept
      {
         if (!opened_)
         {
            return status_t(MDB_NOT_OPEN);
         }
         opened_ = false;
         name_.clear();
         return status_t();
      }

      unsigned int make_mode(bool create) const noexcept
      {
         return create ? MDB_CREATE : 0;
      }

   }; // class table_base_t

   class cursor_t
   {
      MDB_cursor* cursor_{ nullptr };
      store_t& table_;

   public:
      using key_type = std::string;
      using key_reference = key_type&;
      using key_const_reference = const std::string_view&;
      using value_type = std::string;
      using value_reference = value_type&;
      using value_const_reference = const std::string_view&;

      cursor_t() = delete;
      cursor_t(const cursor_t&) = delete;
      cursor_t& operator=(cursor_t&& other) = delete;
      cursor_t& operator=(const cursor_t&) = delete;

      ~cursor_t() noexcept
      {
         close();
      }

      explicit cursor_t(store_t& table) noexcept
         : table_{ table }
      {}

      cursor_t(transaction_t& txn, store_t& table)
         : table_{ table }
      {
         if (status_t status = open(txn); status.nok())
         {
            throw error_t(status);
         }
      }

      status_t open(transaction_t& txn) noexcept
      {
         status_t status{ MDB_ALREADY_OPEN };
         if (cursor_)
         {
            return status;
         }
         if (status = mdb_cursor_open(txn.handle(), table_.handle(), &cursor_); status.nok())
         {
            return status;
         }
         return status;
      }

      status_t close() noexcept
      {
         if (cursor_)
         {
            mdb_cursor_close(cursor_);
            cursor_ = nullptr;
         }
         return status_t();
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
         if (status_t status = get(k, v, MDB_SET); status.nok())
         {
            return status;
         }
         return status_t();
      }

      status_t find(key_const_reference& target_key, key_reference key, value_reference value) noexcept
      {
         key_type k{ target_key };
         if (status_t status = get(k, value, MDB_SET_KEY); status.nok())
         {
            return status;
         }
         key = k;
         return status_t();
      }

      status_t search(key_const_reference& target_key, key_reference key, value_reference value) noexcept
      {
         key_type k{ target_key };
         if (status_t status = get(k, value, MDB_SET_RANGE); status.nok())
         {
            return status;
         }
         key = k;
         return status_t();
      }

      status_t put(key_const_reference key, value_const_reference value) noexcept
      {
         if (!cursor_)
         {
            return status_t(MDB_NOT_OPEN);
         }
         return status_t(mdb_cursor_put(cursor_, data_t(key).data(), data_t(value).data(), 0));
      }

      status_t del() noexcept
      {
         if (!cursor_)
         {
            return status_t(MDB_NOT_OPEN);
         }
         return status_t(mdb_cursor_del(cursor_, 0));
      }

      database_t& database() noexcept
      {
         return table_.database();
      }

      store_t& store() noexcept
      {
         return table_;
      }

      MDB_cursor* handle() noexcept
      {
         return cursor_;
      }

   private:
      status_t get(key_reference key, value_reference value, MDB_cursor_op op) noexcept
      {
         status_t status{ MDB_NOT_OPEN };
         data_t k;
         data_t v;
         if (!cursor_)
         {
            return status;
         }
         if (is_set_operation(op))
         {
            k.set(key);
         }
         if (status = mdb_cursor_get(cursor_, k.data(), v.data(), op); status.nok())
         {
            return status;
         }
         if (op != MDB_SET)
         {
            k.get(key);
            v.get(value);
         }
         return status;
      }

      status_t get(key_reference key, MDB_cursor_op op) noexcept
      {
         status_t status{ MDB_NOT_OPEN };
         data_t k;
         data_t v;
         if (!cursor_)
         {
            return status;
         }
         if (is_set_operation(op))
         {
            k.set(key);
         }
         if (status = mdb_cursor_get(cursor_, k.data(), v.data(), op); status.nok())
         {
            return status;
         }
         if (op != MDB_SET)
         {
            k.get(key);
         }
         return status;
      }

      bool is_set_operation(MDB_cursor_op op) noexcept
      {
         return (op == MDB_SET || op == MDB_SET_KEY || op == MDB_SET_RANGE);
      }
   }; // class cursor_base_t

} // namespace lmdb
