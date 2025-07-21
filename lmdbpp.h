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

#include "lmdb.h"

#include <string>
#include <vector>
#include <span>
#include <string_view>
#include <utility>
#include <stdexcept>
#include <filesystem>
#include <concepts>
#include <type_traits>
#include <cstdint>
#include <cstring>

namespace lmdb {
   
   // error_t: Lightweight LMDB error wrapper.
   //
   // This class encapsulates an LMDB status/error code (int) and provides
   // convenience methods for checking success/failure and retrieving the
   // corresponding error message.
   //
   // Features:
   // - Default-constructs to MDB_SUCCESS (no error).
   // - Implicit conversion to bool: true if no error.
   // - Implicit conversion to int: returns the raw error code.
   // - Provides .ok() to check for success.
   // - Provides .message() to get a human-readable error string via mdb_strerror().
   //
   // Example:
   //
   //   error_t err = some_lmdb_call();
   //   if (!err) {
   //       std::cerr << "Error: " << err.message() << "\n";
   //   }
   class error_t
   {
      int code_{ MDB_SUCCESS };

   public:
      error_t() = default;
      ~error_t() = default;
      error_t(const error_t&) = default;
      error_t(error_t&&) noexcept = default;
      error_t& operator=(const error_t&) = default;
      error_t& operator=(error_t&&) noexcept = default;

      explicit error_t(int code) noexcept
         : code_(code)
      {
      }

      [[nodiscard]] bool ok() const noexcept
      {
         return code_ == MDB_SUCCESS;
      }

      [[nodiscard]] int code() const noexcept
      {
         return code_;
      }

      [[nodiscard]] operator bool() const noexcept
      {
         return ok();
      }

      [[nodiscard]] operator int() const noexcept
      {
         return code_;
      }

      [[nodiscard]] std::string message() const
      {
         return make_msg(code_);
      }

   private:
      static std::string make_msg(int code)
      {
         const char* msg = mdb_strerror(code);
         return msg ? std::string(msg) : "Unknown error code";
      }
   };

   // lmdb_error: Exception class for LMDB errors.
   //
   // This class derives from std::runtime_error and represents an LMDB-specific
   // error. It stores the original LMDB error code and generates a descriptive
   // message using mdb_strerror().
   //
   // Features:
   // - Constructible from an LMDB error code or from an error_t object.
   // - Stores the integer error code for later inspection via .code().
   // - Generates a default error message unless one is explicitly provided.
   //
   // Example:
   //
   //   int rc = mdb_txn_commit(txn);
   //   if (rc != MDB_SUCCESS) {
   //       throw lmdb_error(rc);
   //   }
   //
   //   try {
   //       // ...
   //   } catch (const lmdb_error& e) {
   //       std::cerr << "LMDB error: " << e.what() << " (code: " << e.code() << ")\n";
   //   }
   class lmdb_error : public std::runtime_error
   {
      int err_{ MDB_SUCCESS };

   public:
      explicit lmdb_error(int err)
         : std::runtime_error{ make_msg(err) }
         , err_(err) 
      {}
      
      lmdb_error(int err, const std::string& msg)
         : std::runtime_error(msg), err_(err) 
      {}

      lmdb_error(const error_t& errc)
         : std::runtime_error(errc.message()), err_(errc.code())
      {}

      [[nodiscard]] int code() const noexcept
      { 
         return err_; 
      }

   private:
      static std::string make_msg(int code)
      {
         const char* msg = mdb_strerror(code);
         return msg ? std::string(msg) : "Unknown error code";
      }
   };

   namespace detail {

      // --- Concepts ---
      template<typename T>
      concept TriviallySerializable = std::is_trivially_copyable_v<T>;

      template<typename T>
      concept StringLike = requires(T t) {
         { t.data() } -> std::convertible_to<const char*>;
         { t.size() } -> std::convertible_to<std::size_t>;
      };

      template<typename T>
      concept ByteContainerLike = requires(T t) {
         { t.data() } -> std::convertible_to<const std::byte*>;
         { t.size() } -> std::convertible_to<std::size_t>;
      };

      template<typename T>
      concept SerializableLike = TriviallySerializable<T> || StringLike<T> || ByteContainerLike<T>;

      enum class serializable_kind { trivial, string, bytes, unknown };

      template<typename T>
      constexpr serializable_kind get_serializable_kind() noexcept {
         if constexpr (TriviallySerializable<T>) {
            return serializable_kind::trivial;
         }
         else if constexpr (StringLike<T>) {
            return serializable_kind::string;
         }
         else if constexpr (ByteContainerLike<T>) {
            return serializable_kind::bytes;
         }
         else {
            return serializable_kind::unknown;
         }
      }

      // --- to_mdb_val ---
      template<SerializableLike T>
      [[nodiscard]] inline error_t to_mdb_val(const T& input, MDB_val& out) noexcept {
         constexpr auto kind = get_serializable_kind<T>();

         if constexpr (kind == serializable_kind::trivial) {
            out.mv_size = sizeof(T);
            out.mv_data = const_cast<void*>(static_cast<const void*>(&input));
         }
         else if constexpr (kind == serializable_kind::string) {
            out.mv_size = input.size();
            out.mv_data = const_cast<char*>(input.data());
         }
         else if constexpr (kind == serializable_kind::bytes) {
            out.mv_size = input.size();
            out.mv_data = const_cast<std::byte*>(input.data());
         }
         else {
            out.mv_size = 0;
            out.mv_data = nullptr;
            return error_t(EINVAL); // or define your own invalid error
         }

         return {};
      }

      // --- from_mdb_val ---
      template<typename T>
      [[nodiscard]] inline error_t from_mdb_val(const MDB_val& val, T& out) noexcept {
         constexpr auto kind = get_serializable_kind<T>();

         if constexpr (kind == serializable_kind::trivial) {
            if (val.mv_size != sizeof(T)) return error_t(EPERM);
            std::memcpy(&out, val.mv_data, sizeof(T));
            return {};
         }
         else if constexpr (std::is_same_v<T, std::string>) {
            out.assign(static_cast<const char*>(val.mv_data), val.mv_size);
            return {};
         }
         else if constexpr (std::is_same_v<T, std::string_view>) {
            out = std::string_view(static_cast<const char*>(val.mv_data), val.mv_size);
            return {};
         }
         else if constexpr (std::is_same_v<T, std::vector<std::byte>>) {
            const auto* ptr = static_cast<const std::byte*>(val.mv_data);
            out.assign(ptr, ptr + val.mv_size);
            return {};
         }
         else if constexpr (std::is_same_v<T, std::span<const std::byte>>) {
            out = std::span<const std::byte>(static_cast<const std::byte*>(val.mv_data), val.mv_size);
            return {};
         }
         else {
            return error_t(EINVAL); // unsupported type
         }
      }
   } // namespace detail

   // Common default values used by env_t class
   // High bits reserved for lmdbpp custom flags
   #define MDB_EPHEMERAL   0x10000000  // delete files on close()

   inline static constexpr mdb_mode_t DEFAULT_MODE = 0644; /* -rw-r--r-- */
   inline static constexpr unsigned int DEFAULT_MAXDBS = 128;
   inline static constexpr unsigned int DEFAULT_MAXREADERS = 512;
   inline static constexpr size_t DEFAULT_MMAPSIZE = 2ULL * 1024 * 1024 * 1024; // 2 GiB
   inline static constexpr const char* DEFAULT_NAME = "lmdb.mdb";

   // env_t: RAII-based LMDB environment manager.
   //
   // This class encapsulates the creation, configuration, and lifecycle of an LMDB
   // environment (`MDB_env*`). It provides a safe and flexible interface for setting
   // up the environment, opening it, managing its parameters, and cleaning up resources.
   //
   // Features:
   // * Full RAII management: environment is automatically closed and cleaned up
   // * Configurable before opening: max DBs, max readers, mmap size, file mode, flags
   // * Moveable but non-copyable
   // * Supports both directory-based and single-file environments (via MDB_NOSUBDIR)
   // * Integrates with error_t for safe, explicit error handling
   // * Supports ephemeral environments (custom MDB_EPHEMERAL flag) that delete files on close
   // * Allows manual flushing, reader checks, and stale reader cleanup
   //
   // Usage:
   //   env_t env(MDB_NOSUBDIR, MDB_NOSYNC);
   //   env.path("mydb.mdb");
   //   env.maxdbs(10);
   //   env.mmapsize(1ull << 30); // 1 GiB
   //   if (!env.open()) {
   //       std::cerr << "Failed to open LMDB env: " << env.lasterror().message() << "\n";
   //   }
   //
   // Notes:
   // * If MDB_RDONLY is used, the target LMDB file(s) must already exist
   // * The `open()` function must be called after setting configuration parameters
   // * `close()` releases resources and deletes files if MDB_EPHEMERAL is set
   // * Use `exist()` to check for presence of LMDB files
   //
   // File layout:
   // * If using directories (default): path must point to an existing directory
   //   and LMDB will create `data.mdb` and `lock.mdb` inside it
   // * If using MDB_NOSUBDIR: path should point to a single LMDB file
   class env_t
   {
      MDB_env* envptr_{ nullptr };
      bool isopen_{ false };
      MDB_dbi maxdbs_{ DEFAULT_MAXDBS };
      unsigned int maxreaders_{ DEFAULT_MAXREADERS };
      size_t mapsize_{ DEFAULT_MMAPSIZE };
      mdb_mode_t mode_{ DEFAULT_MODE };
      unsigned int flags_{ 0 };
      std::filesystem::path path_;
      error_t lasterror_;

   public:
      env_t(const env_t&) = delete;
      env_t& operator=(const env_t&) = delete;
      ~env_t() { close(); }

      // create a new lmdb environment object. the creation of environment object fails
      // handle() returns nullptr and lasterror() returns the error code.
      //
      // flags is a variadic template list of flags to be passed to mdb_env_open function.
      // if no flags are passed, the default value of 0 will be used. All flags passed are
      // ORed in and the final result is passed to mdb_env_open. The following flags are
      // suggested:
      //
      //    * MDB_NOSUBDIR Store files directly in the path (as files), not inside a directory
      //    * MDB_RDONLY Open environment in read-only mode (no writes allowed)
      //    * MDB_WRITEMAP Use writable memory map (faster writes, less crash-safe)
      //    * MDB_NOMETASYNC Don’t flush metadata buffers after commit (less safe, faster)
      //    * MDB_NOSYNC Skip flush to disk after commit (even less safe, fastest)
      //    * MDB_MAPASYNC Use async map flushing(requires MDB_NOSYNC)
      //    * MDB_NOTLS Don’t use thread-local storage; transactions must be manually managed per thread
      //    * MDB_NOLOCK Don’t use file locking — use only if you guarantee single-process access
      //    * MDB_NORDAHEAD Disable OS-level read-ahead; better for random access
      //    * MDB_NOMEMINIT Don’t initialize memory before writing (faster, less secure)
      //    * MDB_FIXEDMAP Map memory at a fixed address (dangerous; requires advanced OS support)
      //    * MDB_EPHEMERAL This is a lmdbpp custom flag to indicate files will be deleted on close()
      // 
      // if you are using MDB_RDONLY flag the LMDB file must already exist. You cannot create new
      // new files with MBD_RDONLY flag when open() is invoked

      template<typename... Flags>
      env_t(Flags... flags)
      {
         const unsigned int f = (0 | ... | flags);
         lasterror_ = create();
         if (lasterror_.ok()) flags_ = f;
      }

      // Parameter path indicate the filesystem path where will store its data files. It 
      // must be a directory, unless you use the MDB_NOSUBDIR flag. LMDB will create two
      // files in this directory:
      //    * data.mdb: the database file
      //    * lock.mdb: the reader lock file
      // Note: the directory must already exist. LMDB does not create it.
      // If you use MDB_NOSUBDIR flag then path must a be a path including a filename and 
      // optionally a file extension. MDB_NOSUBDIR tels LMDB to store both data.mdb and lock.mdb
      // as a single file.
      // 
      // flags is a variadic template list of flags to be passed to mdb_env_open function.
      // if no flags are passed, the default value of 0 will be used. All flags passed are
      // ORed in and the final result is passed to mdb_env_open. The following flags are
      // suggested:
      //
      //    * MDB_NOSUBDIR Store files directly in the path (as files), not inside a directory
      //    * MDB_RDONLY Open environment in read-only mode (no writes allowed)
      //    * MDB_WRITEMAP Use writable memory map (faster writes, less crash-safe)
      //    * MDB_NOMETASYNC Don’t flush metadata buffers after commit (less safe, faster)
      //    * MDB_NOSYNC Skip flush to disk after commit (even less safe, fastest)
      //    * MDB_MAPASYNC Use async map flushing(requires MDB_NOSYNC)
      //    * MDB_NOTLS Don’t use thread-local storage; transactions must be manually managed per thread
      //    * MDB_NOLOCK Don’t use file locking — use only if you guarantee single-process access
      //    * MDB_NORDAHEAD Disable OS-level read-ahead; better for random access
      //    * MDB_NOMEMINIT Don’t initialize memory before writing (faster, less secure)
      //    * MDB_FIXEDMAP Map memory at a fixed address (dangerous; requires advanced OS support)
      //    * MDB_EPHEMERAL This is a lmdbpp custom flag to indicate files will be deleted on close()
      // 
      // if you are using MDB_RDONLY flag the LMDB file must already exist. You cannot create new
      // new files with MBD_RDONLY flag when open() is invoked
      // 
      // If the creation of a new environment object fails handle() returns nullptr and 
      // lasterror() returns the error code
      template<typename... Flags>
      env_t(const std::filesystem::path path, Flags... flags)
      {
         const unsigned int f = (0 | ... | flags);
         lasterror_ = create.ok();
         if (lasterror_.ok())
         {
            path_ = path;
            flags_ = f;
         }
      }

      env_t(env_t&& other) noexcept
         : envptr_{ other.envptr_ }
         , isopen_{ std::move(other.isopen_) }
         , maxdbs_{ std::move(other.maxdbs_) }
         , maxreaders_{ std::move(other.maxreaders_) }
         , mapsize_{ std::move(other.mapsize_) }
         , mode_{ std::move(other.mode_) }
         , flags_{ std::move(other.flags_) }
         , path_{ std::move(other.path_) }
         , lasterror_{ std::move(other.lasterror_) }
      {
         other.envptr_ = nullptr;
         other.isopen_ = false;
         other.lasterror_ = error_t();
      }

      env_t& operator=(env_t&& other) noexcept
      {
         if (this != &other)
         {
            cleanup();
            envptr_ = other.envptr_;
            isopen_ = std::move(other.isopen_);
            maxdbs_ = std::move(other.maxdbs_);
            maxreaders_ = std::move(other.maxreaders_);
            mapsize_ = std::move(other.mapsize_);
            mode_ = std::move(other.mode_);
            flags_ = std::move(other.flags_);
            path_ = std::move(other.path_);
            lasterror_ = std::move(other.lasterror_);

            other.envptr_ = nullptr;
            other.isopen_ = false;
            other.lasterror_ = error_t();
         }
         return *this;
      }

      // Opens the LMDB environment on disk, as it maps the database into memory,
      // opens underlying data and lock files and enables actual read/write operations
      // in the environment. Methods maxdbs(), maxreaders(), mmapsize() and mode() should
      // be called before open to change the default values. If the default constructor
      // was used, the LMDB files will be created in the current folder.
      // 
      // The default values are:
      //    * maxdbs - maximum named DBs, default is 128
      //    * maxreaders - maximum read transactions, default is 512
      //    * mapsize - actual mmap (data) size, default is 2 GiB
      //    * mode - file access mode, default is 0644 (-rw-r--r--)
      // 
      // If path is not set and MDB_NOSUBDIR flag is set, the LMDB single file will be created
      // in the default directory with the file name set as lmdb.mdb.
      //
      // if mdb_env_open() fails, the current env pointer is released and cleanup, and a new env 
      // pointer is allocated. If environment is already open, it returns MDB_INVALID
      [[nodiscard]] error_t open()
      {
         lasterror_ = {};
         try
         {
            if (path_.empty())
            {
               path_ = std::filesystem::current_path();
               if (flags_ & MDB_NOSUBDIR)
               {
                  path_ = path_ / DEFAULT_NAME;
               }
            }
         }
         catch (const std::filesystem::filesystem_error&)
         {
            lasterror_ = error_t(MDB_INVALID);
         }
         if (lasterror_.ok())
         {
            if (envptr_ == nullptr)
            {
               if (lasterror_ = create(); !lasterror_.ok()) return lasterror_;;
            }
            if (lasterror_ = init(path_.string(), flags_); !lasterror_.ok())
            {
               cleanup();
               create();
            }
         }
         return lasterror_;
      }

      // close and cleanup the current environment
      void close() noexcept
      {
         cleanup();
         if (flags_ & MDB_EPHEMERAL)
         {
            remove();
         }
      }

      // Returns the configured maximum number of named databases.
      // Can be called at any time.
      [[nodiscard]] MDB_dbi maxdbs() const noexcept
      {
         return maxdbs_;
      }

      // Specifies how many named databases you plan to use (in addition to the 
      // default DB). Must be called before opening the environment. If called after
      // the environment is opened, it returns error MDB_INVALID and the value is
      // not stored.
      [[nodiscard]] error_t maxdbs(MDB_dbi dbs) noexcept
      {
         if (isopen_) return lasterror_ = error_t(MDB_INVALID);
         maxdbs_ = dbs;
         return lasterror_ = error_t();
      }

      // Returns the configured maximum number of readers.
      [[nodiscard]] unsigned int maxreaders() const noexcept
      {
         return maxreaders_;
      }

      // Sets the maximum number of concurrent read-only transactions (readers).
      // Default is 126 on POSIX. Returns MDC_INVALID if called after the environment
      // was opened
      [[nodiscard]] error_t maxreaders(unsigned int readers) noexcept
      {
         if (isopen_) return error_t(MDB_INVALID);
         maxreaders_ = readers;
         return lasterror_ = error_t();
      }

      // Retrieves the current memory map size. Returns 0 if the operation fails
      // after the environment was opened
      [[nodiscard]] size_t mmapsize() const noexcept
      {
         return mapsize_;
      }

      // Sets the maximum size (in bytes) of the memory-mapped region, i.e. maximum 
      // database size. Must be used before mdb_env_open(). Can be increased later if 
      // needed. Returns MDB_INVALID if called after environment was opened.
      [[nodiscard]] error_t mmapsize(size_t size) noexcept
      {
         if (isopen_) return lasterror_ = lasterror_ = error_t(MDB_INVALID);
         mapsize_ = size;
         return lasterror_ = error_t();
      }
      
      // Increases the memory map size to accommodate a larger database.
      [[nodiscard]] error_t mmap_increase(size_t newSize) noexcept
      {
         return {};
      }

      // Retrieve the current file mode
      [[nodiscard]] mdb_mode_t mode() const noexcept
      {
         return mode_;
      }

      // Set the file permissions. Return MDB_INVALID if environment already open
      // The recommended permissions are:
      // 0664 Owner and group read/write
      // 0600 Owner read/write only
      // 0644 Owner read/write others read only. This is the default mode
      [[nodiscard]] error_t mode(mdb_mode_t m) noexcept
      {
         if (isopen_) return lasterror_ = error_t(MDB_INVALID);
         mode_ = m;
         return lasterror_ = error_t();
      }

      // Returns the maximum key length supported (default 511 bytes, or as defined 
      // by compile-time MDB_MAXKEYSIZE).
      [[nodiscard]] int maxkeysize() const noexcept
      {
         return mdb_env_get_maxkeysize(envptr_);
      }

      // Retrieves the filesystem path passed to open(). Returns an empty path
      // if environment is not open or if the operation fails
      [[nodiscard]] std::filesystem::path path() const noexcept
      {
         if (isopen_)
         {
            const char* p{ nullptr };
            if (error_t rc{ mdb_env_get_path(envptr_, &p) }; rc.ok()) return std::filesystem::path(p);
            return {};
         }
         return path_;
      }

      [[nodiscard]] error_t path(std::filesystem::path p) noexcept
      {
         if (isopen_) return error_t(MDB_INVALID);
         path_ = p;
         return {};
      }

      // Returns the current environment flags bitmask. Return -1 if the operation failed
      // Can be called anytime
      [[nodiscard]] unsigned int getflags() const noexcept
      {
         return flags_;
      }

      // set the environment flags. if this method is called while the environment is open
      // it returs MDB_INVALID, otherwise it set the flags and return success.
      template<typename... Flags>
      [[nodiscard]] error_t setflags(Flags... flags)
      {
         if (isopen_) return error_t(MDB_INVALID);
         const unsigned int f = (0 | ... | flags);
         flags_ = f;
         return {};
      }

      // Checks for stale (dead) reader slots in the lock table. This is used to clean up 
      // reader slots left behind by crashed or terminated processes. Behavior:
      // * Only the writer process should call this function.
      // * The check is only meaningful when multiple processes use the environment.
      // * Stale reader slots occur when a process exits without properly calling close().
      // check() returns non-zero() if any dead reader slot were cleared, returns zero
      // if all reader slots are valid, returns -1 if operation failed
      [[nodiscard]] int check() noexcept
      {
         if (!isopen_) return -1;
         int dead{ 0 };
         if (int rc = mdb_reader_check(envptr_, &dead); rc != 0) return -1;
         return dead;
      }

      // Manually flushes the LMDB memory-mapped data to disk — ensuring durability.
      // This is used to :
      // * Force a flush outside of a transaction
      // * Ensure data is persisted immediately(especially useful with flags like MDB_NOSYNC or MDB_MAPASYNC)
      // When force is set to false, the default, flush only if MDB_NOSYNC or MDB_NOMETASYNC was used 
      // during open(). Setting force to true cause it to always flush. Returns EINVAL if called
      // when the environment is not open
      [[nodiscard]] errno_t flush(bool force = true) noexcept
      {
         if (!isopen_) return EINVAL;
         return mdb_env_sync(envptr_, (force ? 1 : 0));
      }

      // Returns true if environment is open
      [[nodiscard]] bool isopen() const noexcept
      {
         return isopen_;
      }

      // retrieve the error result of the last operation
      [[nodiscard]] error_t lasterror() const noexcept
      {
         return lasterror_;
      }

      // return the lmdb environment handle. Can be called at any time.
      // handle() returns nullptr is the environment was not yet created.
      [[nodiscard]] MDB_env* handle() const noexcept
      {
         return envptr_;
      }

      // Return true of LMDB file(s) exists, return false otherwise
      // these function only work if the environment is open, or after it
      // is closed
      [[nodiscard]] bool exist() const noexcept
      {
         namespace fs = std::filesystem;
         try
         {
            if (flags_ & MDB_NOSUBDIR)
            {
               if (fs::exists(path_) && fs::is_regular_file(path_)) return true;
            }
            else if (fs::exists(path_) && fs::is_directory(path_))
            {
               std::filesystem::path datafile{ path_ / "data.mdb" };
               std::filesystem::path lockfile{ path_ / "lock.mdb" };

               if (fs::exists(datafile) && fs::is_regular_file(datafile) &&
                  fs::exists(lockfile) && fs::is_regular_file(lockfile))
               {
                  return true;
               }
            }
         }
         catch (const std::filesystem::filesystem_error&)
         { 
            // do nothing here, just let the function return false
         }
         return false;
      }

      // remove LMDB files if the environment is closed
      bool remove() const noexcept
      {
         namespace fs = std::filesystem;
         try
         {
            if (!isopen_ && exist())
            {
               if (flags_ & MDB_NOSUBDIR)
               {
                  if (fs::remove(path_)) return true;
               }
               else
               {
                  std::filesystem::path datafile{ path_ / "data.mdb" };
                  std::filesystem::path lockfile{ path_ / "lock.mdb" };
                  if (fs::remove(datafile) && fs::remove(lockfile))
                  {
                     return true;
                  }
               }
            }
         }
         catch (const std::filesystem::filesystem_error&)
         {
            // do nothing here, just let the function return false
         }
         return false;
      }

   private:
      error_t create() noexcept
      {
         cleanup();
         if (error_t rc{ mdb_env_create(&envptr_) }; !rc.ok())
         {
            envptr_ = nullptr;
            return rc;
         }
         return {};
      }

      void cleanup() noexcept
      {
         if (envptr_)
         {
            mdb_env_close(envptr_);
            isopen_ = false;
            envptr_ = nullptr;
         }
      }

      error_t init(const std::string& path, unsigned int flags)
      {
         if (isopen_) return error_t(MDB_INVALID);
         // set maxdbs
         if (error_t rc{ mdb_env_set_maxdbs(envptr_, maxdbs_) }; !rc.ok()) return rc;
         // set maxreaders
         if (error_t rc{ mdb_env_set_maxreaders(envptr_, maxreaders_) }; !rc.ok()) return rc;
         // set mmapsize
         if (error_t rc{ mdb_env_set_mapsize(envptr_, mapsize_)}; !rc.ok()) return rc;
         // open the environment
         // make sure we do not pass MDB_EPHEMERAL flag to LMDB
         if (error_t rc{ mdb_env_open(envptr_, path.c_str(), (flags & ~MDB_EPHEMERAL), mode_) }; !rc.ok()) return rc;
         isopen_ = true;
         return error_t();
      }
   };

   // txn_t: RAII-style wrapper for LMDB transactions.
   //
   // This class wraps an MDB_txn* and manages its lifetime using RAII. It supports
   // both read-only and read-write transactions, with automatic cleanup:
   //
   // - On destruction, the transaction is aborted unless it was explicitly committed.
   // - Supports move semantics to transfer ownership safely.
   // - Provides access to the raw MDB_txn* pointer for LMDB calls.
   // - For read-only transactions, supports reset() and renew() to reuse the object
   //   without holding open a long-lived snapshot.
   //
   // Example usage:
   //
   //   txn_t txn(env, txn_t::mode::read_write);
   //   mdb_put(txn.get(), dbi, &key, &val, 0);
   //   txn.commit();
   //
   // For read-only transactions:
   //
   //   txn_t txn(env, txn_t::mode::read_only);
   //   // read something...
   //   txn.reset();  // release snapshot
   //   // wait or do other work...
   //   txn.renew();  // reacquire fresh snapshot
   //
   // Notes:
   // - Only one write transaction may be active in an environment at a time.
   // - Multiple read-only transactions can exist concurrently.
   // - reset() and renew() are only valid on read-only transactions.
   class txn_t
   {
   public:
      enum class type_t { readonly, readwrite };

   private:
      MDB_env* envptr_{ nullptr };
      MDB_txn* txnptr_{ nullptr };
      bool committed_{ true };
      type_t type_{ type_t::readonly };

   public:
      txn_t() = delete;

      // No copy allowed
      txn_t(const txn_t&) = delete;
      txn_t& operator=(const txn_t&) = delete;

      // Move allowed
      txn_t(txn_t&& other) noexcept
         : envptr_{ other.envptr_ }
         , txnptr_{ other.txnptr_ }
         , committed_{ other.committed_ }
         , type_{ other.type_ }
      {
         other.envptr_ = nullptr;
         other.txnptr_ = nullptr;
         other.committed_ = true;
      }
      
      txn_t& operator=(txn_t&& other) noexcept
      {
         if (this != &other)
         {
            cleanup();
            envptr_ = other.envptr_;
            txnptr_ = other.txnptr_;
            committed_ = other.committed_;
            type_ = other.type_;
            // reset the other object
            other.envptr_ = nullptr;
            other.txnptr_ = nullptr;
            other.committed_ = true;
         }
         return *this;  
      }

      // Constructs a txn_t object with the given environment and transaction type.
      //
      // Parameters:
      // - env:   The LMDB environment to associate with the transaction.
      // - type:  The transaction type (read-only by default, or read-write).
      //
      // Behavior:
      // - Stores a pointer to the environment handle.
      // - Initializes the transaction pointer to null and marks it as committed.
      // - Does not begin the transaction automatically — begin() must be called separately.
      // - No parent transaction is used (suitable for top-level transactions only).
      //
      // Notes:
      // - Use this constructor for top-level transactions only.
      explicit txn_t(const env_t& env, type_t type = type_t::readonly)
         : envptr_{ env.handle() }
         , txnptr_{ nullptr }
         , committed_{ true }
         , type_{ type }
      {
      }

      // Begins a new LMDB transaction using the stored environment, type, and optional parent.
      //
      // Returns:
      // - MDB_SUCCESS (wrapped in error_t) on success
      // - MDB_INVALID if the environment pointer is null
      // - MDB_BAD_TXN if the transaction is already active or if a write transaction has a parent
      //
      // Behavior:
      // - Allocates a new MDB_txn and stores it in txnptr_
      // - Uses MDB_RDONLY flag if the transaction type is read-only
      // - Only one write transaction may exist at a time
      // - Sets committed_ to false; caller must later commit or abort the transaction
      //
      // Notes:
      // - This function must not be called more than once per txn_t instance without committing or aborting first.
      [[nodiscard]] error_t begin() noexcept
      {
         // Environment must be valid
         if (envptr_ == nullptr) return error_t(MDB_INVALID);

         // Transaction already active
         if (txnptr_ != nullptr) return error_t(MDB_BAD_TXN);

         unsigned int flags = (type_ == type_t::readonly) ? MDB_RDONLY : 0;
         MDB_txn* ptr = nullptr;

         if (error_t rc{ mdb_txn_begin(envptr_, nullptr, flags, &ptr) }; !rc.ok()) return rc;
         txnptr_ = ptr;
         committed_ = false;
         return {};
      }

      // Commits the current LMDB transaction.
      //
      // Returns:
      // - MDB_SUCCESS (wrapped in error_t) on success
      // - MDB_BAD_TXN if there is no active transaction
      // - Any error returned by mdb_txn_commit if the commit fails
      //
      // Behavior:
      // - Commits all changes made during the transaction to the database
      // - Marks the transaction as committed and resets the internal pointer
      //
      // Notes:
      // - After a successful commit, the transaction is no longer valid and cannot be reused
      // - If the commit fails, the transaction remains active and must be aborted manually
      // - Should only be called on a transaction that was successfully started with begin()
      [[nodiscard]] error_t commit() noexcept
      {
         if (txnptr_ == nullptr) return error_t(MDB_BAD_TXN);
         if (error_t rc{ mdb_txn_commit(txnptr_) }; !rc.ok()) return rc;
         committed_ = true;
         txnptr_ = nullptr; // reset the pointer after commit
         return {};
      }

      // Aborts the current LMDB transaction and discards any changes.
      //
      // Returns:
      // - MDB_SUCCESS (wrapped in error_t) on success
      // - MDB_BAD_TXN if no transaction is currently active
      //
      // Behavior:
      // - Calls mdb_txn_abort() to cancel the transaction
      // - Resets the internal transaction pointer to null
      // - Marks the transaction as committed to suppress cleanup in the destructor
      //
      // Notes:
      // - This function is safe to call multiple times in a guarded context, but will return an error if the transaction is already null
      // - After aborting, the transaction object cannot be reused without calling begin() again
      [[nodiscard]] error_t abort() noexcept
      {
         if (txnptr_ == nullptr) return error_t(MDB_BAD_TXN);
         mdb_txn_abort(txnptr_);
         txnptr_ = nullptr; // reset the pointer after abort
         committed_ = true;
         return {};
      }

      // Resets a read-only transaction, releasing its snapshot while keeping the transaction object valid.
      //
      // Returns:
      // - MDB_SUCCESS (wrapped in error_t) on success
      // - MDB_INVALID if there is no active transaction
      // - MDB_BAD_TXN if the transaction is not read-only
      //
      // Behavior:
      // - Releases the internal snapshot held by the read-only transaction
      // - The transaction must not be used again until renewed with renew()
      // - The transaction object remains valid and can be reused later
      //
      // Notes:
      // - Only valid for read-only transactions (MDB_RDONLY)
      // - After reset(), any access to the transaction (e.g., mdb_get) is invalid until renew() is called
      [[nodiscard]] error_t reset() noexcept
      {
         if (txnptr_ == nullptr) return error_t(MDB_INVALID);
         if (type_ != type_t::readonly) return error_t(MDB_BAD_TXN);
         mdb_txn_reset(txnptr_);
         committed_ = false;
         return {};
      }

      // Renews a previously reset read-only transaction, assigning it a fresh snapshot.
      //
      // Returns:
      // - MDB_SUCCESS (wrapped in error_t) on success
      // - MDB_INVALID if there is no transaction object to renew
      // - MDB_BAD_TXN if the transaction is not read-only
      //
      // Behavior:
      // - Reinitializes a reset read-only transaction so it can be used again
      // - The renewed transaction reflects the current state of the environment
      // - Resets committed_ to false to indicate the transaction is active
      //
      // Notes:
      // - Only valid for transactions created with MDB_RDONLY
      // - Must be preceded by a call to reset(); otherwise, behavior is undefined
      // - After renew(), normal read-only operations (mdb_get, etc.) can resume
      [[nodiscard]] error_t renew() noexcept
      {
         if (txnptr_ == nullptr) return error_t(MDB_INVALID);
         if (type_ != type_t::readonly) return error_t(MDB_BAD_TXN);
         mdb_txn_renew(txnptr_);
         committed_ = false;
         return {};
      }

      // Returns the type of the transaction(read - only or read - write).
      //
      // Useful for checking the transaction mode before performing operations
      // that are only valid for one type (e.g., reset/renew for read-only).
      [[nodiscard]] type_t type() const noexcept
      {
         return type_;
      }

      // Returns true if a transaction is currently active and has not been committed.
      //
      // This indicates that a transaction has been started (i.e., txnptr_ is valid),
      // but commit() has not yet been called. It can be used to determine whether
      // cleanup (e.g., abort) is required before starting a new transaction or during destruction.
      //
      // Notes:
      // - Returns false if no transaction is active or if the last transaction was committed.
      // - Use this to enforce proper transaction lifecycle management.
      [[nodiscard]] bool pending() const noexcept
      {
         return txnptr_ != nullptr && !committed_;
      }

      // Returns the raw MDB_txn* handle.
      //
      // This allows direct access to the LMDB C API for operations that require the native transaction pointer.
      // Caution: Should be used carefully to avoid bypassing RAII safety or violating transaction invariants.
      [[nodiscard]] MDB_txn* handle() const noexcept
      {
         return txnptr_;
      }

   private:
      void cleanup() noexcept
      {
         if (txnptr_ && !committed_)
         {
            mdb_txn_abort(txnptr_);
         }
         txnptr_ = nullptr;
         committed_ = true;
      }
   };

   // dbi_t: lightweight RAII wrapper for an LMDB database handle (MDB_dbi)
   //
   // This class represents a named or unnamed LMDB database inside an open environment.
   // It provides typed, type-safe access to key/value operations using templates constrained
   // by SerializableLike concepts.
   //
   // Features:
   // - Move-only (non-copyable) ownership of the MDB_dbi handle
   // - Open/close support with optional flags (e.g., MDB_CREATE, MDB_DUPSORT)
   // - Typed put/get/delete methods using any trivially copyable, string-like, or byte-span types
   // - Variadic flag support for put()
   // - Safe stat() query for page, entry, and B-tree statistics
   // - Key and value comparison wrappers (mdb_cmp, mdb_dcmp)
   // - Support for installing custom comparators (mdb_set_compare, mdb_set_dupsort)
   //
   // Notes:
   // - All operations require an explicit transaction (txn_t), which must be active
   // - Use erase() to clear the DB contents, and drop() to delete the DB entirely
   // - Use isopen() to verify the handle is valid before using it
   class dbi_t
   {
      MDB_dbi dbi_{ 0 };
      bool opened_{ false };

   public:
      dbi_t() = default;
      ~dbi_t() = default;
      dbi_t(const dbi_t&) = delete;
      dbi_t& operator=(const dbi_t&) = delete;

      dbi_t(dbi_t&& other) noexcept
         : dbi_{ other.dbi_ }
         , opened_{ other.opened_ }
      {
         other.opened_ = false;
      }

      dbi_t& operator=(dbi_t&& other) noexcept
      {
         if (this != &other)
         {
            dbi_ = other.dbi_;
            opened_ = other.opened_;
            other.opened_ = false;
         }
         return *this;
      }

      // open: Opens an LMDB database handle within a transaction.
      //
      // Parameters:
      // - txn   : an active transaction (must be read-write if using MDB_CREATE)
      // - name  : optional database name (empty string means the unnamed default DB)
      // - flags : optional list of LMDB flags to control database behavior
      //
      // Supported flag values (combine as needed):
      // - MDB_REVERSEKEY   : keys are compared in reverse byte order
      // - MDB_DUPSORT      : allow duplicate keys (sorted by value)
      // - MDB_INTEGERKEY   : keys are binary integers, compared numerically
      // - MDB_DUPFIXED     : duplicate values are all the same size (requires DUPSORT)
      // - MDB_INTEGERDUP   : duplicate values are binary integers, compared numerically
      // - MDB_REVERSEDUP   : duplicate values are compared in reverse byte order
      // - MDB_CREATE       : create the database if it does not already exist
      //
      // Behavior:
      // - Opens or creates a named or unnamed database using mdb_dbi_open()
      // - Sets the internal dbi_ handle if successful
      // - Prevents re-opening an already open DB handle (returns MDB_BAD_DBI)
      //
      // Notes:
      // - Must be called before any other operation (put/get/del/stat/etc.)
      // - MDB_CREATE must not be used in read-only transactions
      // - Passing no flags is equivalent to opening an existing DB in default mode
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_DBI if this dbi_t is already open
      // - MDB_BAD_TXN if the transaction handle is null
      // - MDB_NOTFOUND if the DB doesn’t exist and MDB_CREATE is not specified
      // - Other LMDB error codes on failure
      template<typename... Flags>
      [[nodiscard]] error_t open(txn_t& txn, std::string_view name = {}, Flags... flags) noexcept
      {
         const unsigned int flg = (0 | ... | flags);
         if (opened_) return error_t(MDB_BAD_DBI); // avoid re-open
         if (!txn.handle()) return error_t(MDB_BAD_TXN);

         const char* dbname = name.empty() ? nullptr : name.data();
         error_t rc{ mdb_dbi_open(txn.handle(), dbname, flg, &dbi_) };
         opened_ = rc.ok();
         return rc;
      }

      // erase: Clears all key-value pairs from the database without deleting the database itself.
      //
      // Parameters:
      // - txn : an active transaction (must be read-write)
      //
      // Behavior:
      // - Invokes mdb_drop with the 'del' parameter set to 0, which erases the contents
      //   of the database but keeps the DBI handle valid and usable afterward.
      //
      // Notes:
      // - The database remains open and can still be used for put/get operations.
      // - This is equivalent to clearing all entries without dropping the schema.
      // - The transaction must be valid and writable.
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_DBI if the database is not open or the transaction is invalid
      // - Other LMDB error codes on failure
      [[nodiscard]] error_t erase(txn_t& txn) const noexcept
      {
         if (!opened_ || !txn.handle()) return error_t(MDB_BAD_DBI);
         return error_t(mdb_drop(txn.handle(), dbi_, 0));
      }

      // drop: Permanently deletes the database and invalidates the DBI handle.
      //
      // Parameters:
      // - txn : an active transaction (must be read-write)
      //
      // Behavior:
      // - Calls mdb_drop with the 'del' parameter set to 1, which deletes the database
      //   from the environment and invalidates the associated DBI handle.
      // - If successful, this method marks the dbi_t instance as closed (opened_ = false).
      //
      // Notes:
      // - After drop(), this dbi_t instance must not be used again until reopened.
      // - Any named database can be dropped this way; for unnamed DBs, behavior is valid
      //   but should be used with caution.
      // - All data and metadata associated with the DB are permanently removed.
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_DBI if the database is not open or the transaction is invalid
      // - Other LMDB error codes on failure
      [[nodiscard]] error_t drop(txn_t& txn) noexcept
      {
         if (!opened_ || !txn.handle()) return error_t(MDB_BAD_DBI);
         error_t rc{ mdb_drop(txn.handle(), dbi_, 1) };
         if (rc.ok()) opened_ = false;
         return rc;
      }

      // compare_keys: Compares two keys using LMDB's key ordering for this database.
      //
      // Template Parameters:
      // - KeyT : must satisfy SerializableLike (e.g., int, string_view, span<byte>, etc.)
      //
      // Parameters:
      // - txn    : an active transaction (read-only or read-write)
      // - a, b   : two keys to compare
      // - result : output integer where comparison result is stored:
      //            - result < 0 if a < b
      //            - result == 0 if a == b
      //            - result > 0 if a > b
      //
      // Behavior:
      // - Converts both keys to MDB_val using to_mdb_val()
      // - Invokes mdb_cmp() with the current database handle
      //
      // Notes:
      // - The database must be open and the transaction must be valid.
      // - Key ordering is determined by LMDB's default or any installed custom comparator.
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_TXN if the transaction or database is not valid
      template<detail::SerializableLike KeyT>
      [[nodiscard]] error_t compare_keys(const txn_t& txn, const KeyT& a, const KeyT& b, int& result) const noexcept 
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);

         MDB_val va = detail::to_mdb_val(a);
         MDB_val vb = detail::to_mdb_val(b);

         result = mdb_cmp(txn.handle(), dbi_, &va, &vb);
         return {};
      }

      // compare_values: Compares two duplicate values using LMDB's dup sort order.
      //
      // Template Parameters:
      // - ValueT : must satisfy SerializableLike (e.g., int, string_view, vector<byte>, etc.)
      //
      // Parameters:
      // - txn    : an active transaction (read-only or read-write)
      // - a, b   : two values associated with the same key
      // - result : output integer where comparison result is stored:
      //            - result < 0 if a < b
      //            - result == 0 if a == b
      //            - result > 0 if a > b
      //
      // Behavior:
      // - Converts both values to MDB_val using to_mdb_val()
      // - Calls mdb_dcmp() to compare them according to the DB’s duplicate sort rules
      //
      // Notes:
      // - The database must be open and must have been opened with MDB_DUPSORT.
      // - The transaction must be valid.
      // - A custom comparator may be installed using set_value_compare().
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_TXN if the database is not open or the transaction is invalid
      template<detail::SerializableLike ValueT>
      [[nodiscard]] error_t compare_values(const txn_t& txn, const ValueT& a, const ValueT& b, int& result) const noexcept 
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);

         MDB_val va = detail::to_mdb_val(a);
         MDB_val vb = detail::to_mdb_val(b);

         result = mdb_dcmp(txn.handle(), dbi_, &va, &vb);
         return error_t();
      }

      // set_key_compare: Installs a custom key comparator function for this database.
      //
      // Parameters:
      // - txn : an active transaction (read-only or read-write)
      // - cmp : a pointer to an MDB_cmp_func that defines custom key comparison logic
      //
      // Behavior:
      // - Calls mdb_set_compare() to override the default key ordering logic.
      // - Must be called immediately after opening the database, before any read/write operations.
      //
      // Notes:
      // - All users of the database must use the same comparison function.
      // - The database must have been opened but not yet accessed.
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_DBI if the database is not open
      [[nodiscard]] error_t set_key_compare(txn_t& txn, MDB_cmp_func* cmp) noexcept
      {
         if (!opened_) return error_t(MDB_BAD_DBI);
         return error_t(mdb_set_compare(txn.handle(), dbi_, cmp));
      }

      // set_value_compare: Installs a custom comparator for duplicate values.
      //
      // Parameters:
      // - txn : an active transaction (read-only or read-write)
      // - cmp : a pointer to an MDB_cmp_func for comparing duplicate values
      //
      // Behavior:
      // - Calls mdb_set_dupsort() to customize value sorting in MDB_DUPSORT databases.
      // - Must be called immediately after opening the database, before any operations.
      //
      // Notes:
      // - Only applicable to databases opened with MDB_DUPSORT.
      // - All users of the database must use the same value comparator.
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_DBI if the database is not open
      [[nodiscard]] error_t set_value_compare(txn_t& txn, MDB_cmp_func* cmp) noexcept
      {
         if (!opened_) return error_t(MDB_BAD_DBI); 
         return error_t(mdb_set_dupsort(txn.handle(), dbi_, cmp));
      }

      // isopen: Returns true if the database is currently open.
      //
      // Returns:
      // - true  if dbi_t holds a valid, opened MDB_dbi handle
      // - false if the handle is uninitialized or has been dropped
      [[nodiscard]] bool isopen() const noexcept
      {
         return opened_;
      }

      // handle: Returns the raw MDB_dbi handle.
      //
      // Returns:
      // - The internal MDB_dbi associated with the open database
      //
      // Notes:
      // - Useful for lower-level access, such as cursor operations
      // - Safe to call regardless of open state
      [[nodiscard]] MDB_dbi handle() const noexcept
      {
         return dbi_;
      }

      // stat: Retrieves LMDB database statistics for this DBI handle.
      //
      // Parameters:
      // - txn : an active transaction (read-only or read-write)
      // - out : output reference to an MDB_stat struct that will be filled with DB metadata
      //
      // Behavior:
      // - Calls mdb_stat() to collect information like page size, entry count, and B-tree depth.
      //
      // Notes:
      // - Does not modify the database
      // - Requires the database to be open and the transaction to be valid
      //
      // Returns:
      // - MDB_SUCCESS (0) on success
      // - MDB_BAD_TXN if the transaction or database is not valid
      [[nodiscard]] error_t stat(const txn_t& txn, MDB_stat& out) const noexcept
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);
         return error_t(mdb_stat(txn.handle(), dbi_, &out));
      }

      // put: Stores or updates a key-value pair in the database.
      //
      // Template Parameters:
      // - KeyT   : type of the key (must satisfy SerializableLike)
      // - ValueT : type of the value (must satisfy SerializableLike)
      // - Flags  : optional variadic list of LMDB put flags
      //
      // Parameters:
      // - txn   : an active transaction (must be read-write)
      // - key   : the key to store
      // - value : the value to associate with the key
      // - flags : optional flags (e.g., MDB_NOOVERWRITE, MDB_APPEND, etc.)
      //
      // Supported Flags:
      // - MDB_NOOVERWRITE : return MDB_KEYEXIST if key already exists
      // - MDB_NODUPDATA   : for MDB_DUPSORT DBs, fail if duplicate value exists
      // - MDB_RESERVE     : reserve space instead of copying value immediately
      // - MDB_APPEND      : optimize by appending keys in sorted order
      // - MDB_APPENDDUP   : append duplicates in sorted order
      //
      // Returns:
      // - MDB_SUCCESS on success
      // - MDB_KEYEXIST if key exists and MDB_NOOVERWRITE is used
      // - MDB_BAD_TXN if transaction or DB is not valid
      // - Other LMDB error codes on failure
      template<detail::SerializableLike KeyT, detail::SerializableLike ValueT, typename... Flags>
      [[nodiscard]] error_t put(const txn_t& txn, const KeyT& key, const ValueT& value, Flags... flags) const noexcept
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);
         unsigned int combined_flags = (0 | ... | flags);

         MDB_val k, v;
         if (auto err = detail::to_mdb_val(key, k); !err.ok()) return err;
         if (auto err = detail::to_mdb_val(value, v); !err.ok()) return err;

         return error_t(mdb_put(txn.handle(), dbi_, &k, &v, combined_flags));
      }

      // get: Retrieves a value associated with a key from the database.
      //
      // Template Parameters:
      // - KeyT   : type of the key (must satisfy SerializableLike)
      // - ValueT : type of the output value (must satisfy SerializableLike)
      //
      // Parameters:
      // - txn       : an active transaction (read-only or read-write)
      // - key       : the key to look up
      // - out_value : reference to receive the resulting value
      //
      // Behavior:
      // - Converts key to MDB_val and queries with mdb_get
      // - Converts result MDB_val back into ValueT
      //
      // Returns:
      // - MDB_SUCCESS on success
      // - MDB_NOTFOUND if key does not exist
      // - MDB_BAD_TXN if transaction or DB is not valid
      // - MDB_INVALID if value type doesn't match data size
      template<detail::SerializableLike KeyT, detail::SerializableLike ValueT>
      [[nodiscard]] error_t get(const txn_t& txn, const KeyT& key, ValueT& out_value) const noexcept
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);

         MDB_val k;
         if (auto err = detail::to_mdb_val(key, k); !err.ok()) return err;

         MDB_val result;
         if (int rc = mdb_get(txn.handle(), dbi_, &k, &result); rc != MDB_SUCCESS)
            return error_t(rc);

         return detail::from_mdb_val(result, out_value);
      }

      // del: Deletes all values associated with a key.
      //
      // Template Parameters:
      // - KeyT : type of the key (must satisfy SerializableLike)
      //
      // Parameters:
      // - txn : an active transaction (must be read-write)
      // - key : the key to delete
      //
      // Behavior:
      // - Removes all entries matching the key from the database.
      // - For MDB_DUPSORT DBs, this removes all duplicate values too.
      //
      // Returns:
      // - MDB_SUCCESS on success
      // - MDB_NOTFOUND if key does not exist
      // - MDB_BAD_TXN if transaction or DB is not valid
      template<detail::SerializableLike KeyT>
      [[nodiscard]] error_t del(const txn_t& txn, const KeyT& key) const noexcept
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);

         MDB_val k;
         if (auto err = detail::to_mdb_val(key, k); !err.ok()) return err;

         return error_t(mdb_del(txn.handle(), dbi_, &k, nullptr));
      }


      // del: Deletes a specific key-value pair in MDB_DUPSORT databases.
      //
      // Template Parameters:
      // - KeyT   : type of the key (must satisfy SerializableLike)
      // - ValueT : type of the value (must satisfy SerializableLike)
      //
      // Parameters:
      // - txn   : an active transaction (must be read-write)
      // - key   : the key to search for
      // - value : the specific value to delete under the key
      //
      // Behavior:
      // - If the DB supports duplicate values (MDB_DUPSORT), this removes
      //   only the exact key+value pair.
      //
      // Returns:
      // - MDB_SUCCESS on success
      // - MDB_NOTFOUND if the key or key+value pair does not exist
      // - MDB_BAD_TXN if transaction or DB is not valid
      template<detail::SerializableLike KeyT, detail::SerializableLike ValueT>
      [[nodiscard]] error_t del(const txn_t& txn, const KeyT& key, const ValueT& value) const noexcept
      {
         if (!opened_ || txn.handle() == nullptr) return error_t(MDB_BAD_TXN);

         MDB_val k, v;
         if (auto err = detail::to_mdb_val(key, k); !err.ok()) return err;
         if (auto err = detail::to_mdb_val(value, v); !err.ok()) return err;

         return error_t(mdb_del(txn.handle(), dbi_, &k, &v));
      }
   };

} // namespace lmdb
