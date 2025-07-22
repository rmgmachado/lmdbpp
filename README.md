# lmdbpp
C++ warpper for Symas LMDB key/value pair database.

LMDB is a B+tree-based database management library modeled loosely on the BerkeleyDB API, but much simplified. The entire database is exposed in a memory map, and all data fetches return data directly from the mapped memory, so no malloc's or memcpy's occur during data fetches. As such, the library is extremely simple because it requires no page caching layer of its own, and it is extremely high performance and memory-efficient. It is also fully transactional with full ACID semantics, and when the memory map is read-only, the database integrity cannot be corrupted by stray pointer writes from application code.

The library is fully thread-aware and supports concurrent read/write access from multiple processes and threads. Data pages use a copy-on- write strategy so no active data pages are ever overwritten, which also provides resistance to corruption and eliminates the need of any special recovery procedures after a system crash. Writes are fully serialized; only one write transaction may be active at a time, which guarantees that writers can never deadlock. The database structure is multi-versioned so readers run with no locks; writers cannot block readers, and readers don't block writers.

Unlike other well-known database mechanisms which use either write-ahead transaction logs or append-only data writes, LMDB requires no maintenance during operation. Both write-ahead loggers and append-only databases require periodic checkpointing and/or compaction of their log or database files otherwise they grow without bound. LMDB tracks free pages within the database and re-uses them for new write operations, so the database size does not grow without bound in normal use.

LMDB is part of the [openLDAP Project][openldap]. openLDAP is a product of [Symas Corporation][symas].

[openldap]: https://www.openldap.org/
[symas]: https://www.symas.com/welcome-to-symas

References: [LMDB source code Github][lmdb source], [LMDB documentation link][lmdb doc], [LMDB Getting Started Guide][lmdb guide], [LMDB API Reference][lmdb reference]

[lmdb source]: https://github.com/LMDB/lmdb
[lmdb doc]: http://www.lmdb.tech/doc/index.html
[lmdb guide]: http://www.lmdb.tech/doc/starting.html
[lmdb reference]: http://www.lmdb.tech/doc/group__mdb.html

## LMDB Features

* Key/Value store using B+trees
* Fully transactional, ACID compliant
* MVCC, readers never block
* Uses memory-mapped files, needs no tuning
* Crash-proof, no recovery needed after restart
* Highly optimized, extremely compact - C code under 40kb object code
* Runs on most modern OSs - Linux, Android, MacOSX, *BSD, Windows, etc

### Concurrency Support

* Both multi-process and multi-thread
* Single Writer + N Readers
   * Writers don't block readers
   * Readers don't block writers
   * Reads scale perfectly linearly with available CPU cores
* Nested transactions
* Batched writes

### Uses Copy-on-Write
* Live data is never overwritten
* Database structure cannot be corrupted by incomplete operations (system crashes)
* No write-ahead logs needed
* No transaction log cleanup/maintenance
* No recovery needed after crashes

### Uses Single-Level_Store
* Reads are satisfied directly from the memory map - no malloc or memcpy overhead
* Writes can be performanced directly to the memory map - no write buffers, no buffer tuning
* Relies on the OS/filesystem cache - no wasted memory in app-level caching
* Can store live pointer-bases objects directly

## Caveats using LMDB

* A broken lock file can cause sync issues. Stale reader transactions left behind by an aborted program cause further writes to grow the database quickly, and stale locks can block further operations. To fix this issue check for stale readers periodically using database_t::check() method.
* Only the database file owner should normally use the database on BSD systems.
* A thread can only use one transaction at a time. Please see LMDB documentation for more details
* Use database_t in the process which started it, without fork()ing
* Do not open a LMDB database twice in the same process. It breaks flock() advisory locking.
* Avoid long-lived transactions. Read transactions prevent reuse of pages freeed by newer write transactions, thus the database can grow quickly. Write transactions prevent other write transactions, since writes are serialized.
* Avoid suspending a process with active transactions. These would then be "long-lived" as aboce.
* Avoid aborting a process with an active transaction. These would be "long-lived" as above until a database_t::check() is called for cleanup stale readers.
* Close the database once in a while, so the lockfile can get reset.
* Do not use LMDB databases on remote filesystems, even between processes on the same host. This breaks flock() advisory locs on some OSes, possibly memory map suunc, and certainly sync between programs on different hosts.
* Opening a database can fail if another process is opening or closing it at exact same time

### LMDB Performance

The charts below how the performance of LMDB, represented by the MDB collumn, when compared to similar key/value store databases. The charts shown below extracted from Howard Chu presentations and from Syma Corporation's [In-Memory Microbenchmark](http://www.lmdb.tech/bench/inmem/) and [Database Microbenchmarks](http://www.lmdb.tech/bench/microbench/):

<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-read-performance-sr.png" width="700" height="300" border="10"/>
<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-read-performance-lr.png" width="700" height="300" border="10"/>
<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-write-performance-lr.png" width="700" height="300" border="10"/>

## lmdbpp C++ wrapper

lmdbpp implements a simple C++ wrapper around the LMDB C API. lmdbpp exposes classes under the namespace lmdb representing the major LMDB objects, lmdbpp limitations

The following LMDB features are not yet implemented by lmdbpp wrapper:
* No nested transactions
* No batched writes

### lmdbpp project files

| File | Description |
|--|--|
| lmdbpp.h | C++ wrapper for LMDB API |
| lmdbpp-test.cpp | Catch2 unit test for lmdbpp code |
| lmdb.h | lmdb header file |
| mdb.c | lmdb C source code |
| midl.h | header file used internally by lmdb C source code |
| midl.c | C source code used internally by lmdb |
| catch2.h | Cath2 single header file - unit tests |
| CmakeLists.txt | Cmake build script |

## `env_t` – LMDB Environment Manager

RAII-based C++20 wrapper for `MDB_env*` that manages the lifecycle, configuration, and access to an LMDB environment.

---

### Constructors

#### Default constructor with flags

Constructs a new LMDB environment object and stores the specified flags for later use.

```cpp
template<typename... Flags>
env_t(Flags... flags)
```

**Parameters:**

- `flags`: Variadic list of LMDB flags such as `MDB_NOSUBDIR`, `MDB_RDONLY`, `MDB_NOSYNC`, `MDB_WRITEMAP`, `MDB_EPHEMERAL`.

**Behaviour:**\
Creates an LMDB environment handle with `mdb_env_create` and stores the flag configuration internally.

---

#### Path-based constructor

Constructs a new environment with a specific filesystem path and flags.

```cpp
template<typename... Flags>
env_t(const std::filesystem::path path, Flags... flags)
```

**Parameters:**

- `path`: Filesystem location for the database files (or single file if `MDB_NOSUBDIR` is used).
- `flags`: Optional LMDB flags.

**Behaviour:**\
Initializes the environment handle, sets the path, and stores flags for later use in `open()`.

---

#### Move constructor

Transfers ownership of another environment.

```cpp
env_t(env_t&& other) noexcept
```

**Parameters:**

- `other`: An rvalue reference to another `env_t` instance.

**Behaviour:**\
Moves internal state (pointers, configuration, and flags) from the source and resets the source to default.

---

### Operators

#### env\_t& operator=(env\_t&& other) noexcept

Transfers ownership of another `env_t` environment.

```cpp
env_t& operator=(env_t&& other) noexcept
```

**Parameters:**

- `other`: An rvalue reference to another `env_t` instance.

**Behaviour:**\
Cleans up existing resources, adopts state from `other`, and resets `other`.

---

### Destructor

#### \~env\_t()

Destroys the environment and optionally removes files.

```cpp
~env_t()
```

**Behaviour:**\
Calls `close()`. If `MDB_EPHEMERAL` is set, also removes the LMDB files from disk.

---

### Member Functions

#### error\_t open()

Opens the LMDB environment on disk using the configured parameters.

```cpp
error_t open()
```

**Parameters:**\
None

**Behaviour:**

- Sets default path if not provided.
- Applies `maxdbs`, `maxreaders`, and `mapsize`.
- Calls `mdb_env_open`.

**Returns:**\
`MDB_SUCCESS` on success, or error in `lasterror()`

---

#### void close() noexcept

Closes the environment and deletes files if flagged as ephemeral.

```cpp
void close() noexcept
```

**Parameters:**\
None

**Behaviour:**

- Calls `mdb_env_close`.
- Deletes files if `MDB_EPHEMERAL` is set.

**Returns:**\
None

---

#### MDB\_dbi maxdbs() const noexcept

Returns the configured maximum number of named databases.

```cpp
MDB_dbi maxdbs() const noexcept
```

**Returns:**\
`MDB_dbi` value representing the max DB count

---

#### error\_t maxdbs(MDB\_dbi dbs) noexcept

Sets the maximum number of named databases.

```cpp
error_t maxdbs(MDB_dbi dbs) noexcept
```

**Parameters:**

- `dbs`: Maximum number of named databases

**Behaviour:**\
Fails if environment is already open. Stores value otherwise.

**Returns:**\
`MDB_SUCCESS` or `MDB_INVALID`

---

#### unsigned int maxreaders() const noexcept

Returns the configured number of maximum reader slots.

```cpp
unsigned int maxreaders() const noexcept
```

**Returns:**\
Current max readers configuration

---

#### error\_t maxreaders(unsigned int readers) noexcept

Sets the number of concurrent readers.

```cpp
error_t maxreaders(unsigned int readers) noexcept
```

**Parameters:**

- `readers`: Maximum concurrent readers

**Returns:**\
`MDB_SUCCESS` or `MDB_INVALID`

---

#### size\_t mmapsize() const noexcept

Returns the configured memory map size.

```cpp
size_t mmapsize() const noexcept
```

**Returns:**\
Memory map size in bytes

---

#### error\_t mmapsize(size\_t size) noexcept

Sets the memory map size.

```cpp
error_t mmapsize(size_t size) noexcept
```

**Parameters:**

- `size`: Map size in bytes

**Returns:**\
`MDB_SUCCESS` or `MDB_INVALID`

---

#### error\_t mmap\_increase(size\_t newSize) noexcept

Placeholder function to increase map size.

```cpp
error_t mmap_increase(size_t newSize) noexcept
```

**Returns:**\
Always returns `MDB_SUCCESS`

---

#### mdb\_mode\_t mode() const noexcept

Returns the file mode configured for the environment.

```cpp
mdb_mode_t mode() const noexcept
```

**Returns:**\
POSIX file mode

---

#### error\_t mode(mdb\_mode\_t m) noexcept

Sets the file mode.

```cpp
error_t mode(mdb_mode_t m) noexcept
```

**Parameters:**

- `m`: POSIX mode (e.g. 0644)

**Returns:**\
`MDB_SUCCESS` or `MDB_INVALID`

---

#### int maxkeysize() const noexcept

Returns the maximum supported key size.

```cpp
int maxkeysize() const noexcept
```

**Returns:**\
Maximum key size in bytes

---

#### std::filesystem::path path() const noexcept

Returns the configured or resolved filesystem path.

```cpp
std::filesystem::path path() const noexcept
```

**Returns:**\
Current LMDB path

---

#### error\_t path(std::filesystem::path p) noexcept

Sets the LMDB environment path.

```cpp
error_t path(std::filesystem::path p) noexcept
```

**Parameters:**

- `p`: Path to use

**Returns:**\
`MDB_SUCCESS` or `MDB_INVALID`

---

#### unsigned int getflags() const noexcept

Returns the LMDB flags used.

```cpp
unsigned int getflags() const noexcept
```

**Returns:**\
Bitmask of environment flags

---

#### error\_t setflags(Flags... flags)

Sets the LMDB flags.

```cpp
template<typename... Flags>
error_t setflags(Flags... flags)
```

**Parameters:**

- `flags`: Variadic list of LMDB flags

**Returns:**\
`MDB_SUCCESS` or `MDB_INVALID`

---

#### int check() noexcept

Checks for stale reader slots.

```cpp
int check() noexcept
```

**Returns:**

> 0 if readers cleared, 0 if clean, -1 on failure

---

#### errno\_t flush(bool force = true) noexcept

Flushes memory-mapped data to disk.

```cpp
errno_t flush(bool force = true) noexcept
```

**Parameters:**

- `force`: If true, forces flush

**Returns:**\
`MDB_SUCCESS` or `EINVAL`

---

#### bool isopen() const noexcept

Returns whether the environment is open.

```cpp
bool isopen() const noexcept
```

**Returns:**\
`true` or `false`

---

#### error\_t lasterror() const noexcept

Returns the last error status.

```cpp
error_t lasterror() const noexcept
```

**Returns:**\
`error_t` object

---

#### MDB\_env\* handle() const noexcept

Returns the raw LMDB environment handle.

```cpp
MDB_env* handle() const noexcept
```

**Returns:**\
Pointer to `MDB_env`

---

#### bool exist() const noexcept

Checks whether the LMDB files exist.

```cpp
bool exist() const noexcept
```

**Returns:**\
`true` if files exist, `false` otherwise

---

#### bool remove() const noexcept

Removes the LMDB files.

```cpp
bool remove() const noexcept
```

**Returns:**\
`true` on success, `false` otherwise

## `txn_t` – LMDB Transaction Wrapper

RAII-style C++20 wrapper for `MDB_txn*` that manages read-only and read-write transactions within an LMDB environment.

---

### Constructors

#### Explicit constructor with environment and type

Constructs a transaction wrapper tied to a specific LMDB environment and transaction type.

```cpp
explicit txn_t(const env_t& env, type_t type = type_t::readonly)
```

**Parameters:**

- `env`: Reference to an `env_t` object representing the LMDB environment.
- `type`: Optional transaction type (read-only by default).

**Behaviour:** Stores a pointer to the LMDB environment. Transaction must be explicitly started using `begin()`.

---

#### Move constructor

Transfers ownership from another transaction.

```cpp
txn_t(txn_t&& other) noexcept
```

**Parameters:**

- `other`: Rvalue reference to another `txn_t` instance.

**Behaviour:** Takes ownership of the underlying `MDB_txn*` and environment pointer. Resets `other`.

---

### Operators

#### txn\_t& operator=(txn\_t&& other) noexcept

Transfers ownership from another transaction.

```cpp
txn_t& operator=(txn_t&& other) noexcept
```

**Parameters:**

- `other`: Rvalue reference to another `txn_t` instance.

**Behaviour:** Cleans up current state, transfers ownership from `other`, and resets `other`.

---

### Destructor

#### \~txn\_t()

Aborts the transaction if it hasn't been committed.

```cpp
~txn_t()
```

**Behaviour:** Calls `mdb_txn_abort()` if the transaction is still pending.

---

### Member Functions

#### error\_t begin()

Begins a new transaction in the associated environment.

```cpp
error_t begin()
```

**Parameters:** None

**Behaviour:** Allocates a new LMDB transaction with optional `MDB_RDONLY` flag. Ensures rules on nested transactions are respected.

**Returns:**

- `MDB_SUCCESS` on success
- `MDB_INVALID` or `MDB_BAD_TXN` on failure

---

#### error\_t commit()

Commits the transaction.

```cpp
error_t commit()
```

**Parameters:** None

**Behaviour:** Calls `mdb_txn_commit()` and marks the transaction as committed.

**Returns:**

- `MDB_SUCCESS` on success
- `MDB_BAD_TXN` if not active
- Error code on failure

---

#### error\_t abort()

Aborts the transaction.

```cpp
error_t abort()
```

**Parameters:** None

**Behaviour:** Calls `mdb_txn_abort()` and resets the internal pointer. Marks transaction as committed to suppress destructor cleanup.

**Returns:**

- `MDB_SUCCESS` or `MDB_BAD_TXN`

---

#### error\_t reset()

Releases the read-only snapshot for reuse.

```cpp
error_t reset()
```

**Parameters:** None

**Behaviour:** Valid only for read-only transactions. Resets snapshot using `mdb_txn_reset()`.

**Returns:**

- `MDB_SUCCESS` or appropriate LMDB error

---

#### error\_t renew()

Renews a previously reset read-only transaction.

```cpp
error_t renew()
```

**Parameters:** None

**Behaviour:** Valid only for read-only transactions. Acquires a new snapshot using `mdb_txn_renew()`.

**Returns:**

- `MDB_SUCCESS` or error code

---

#### type\_t type() const noexcept

Returns the transaction type.

```cpp
type_t type() const noexcept
```

**Returns:**

- `type_t::readonly` or `type_t::readwrite`

---

#### bool pending() const noexcept

Checks if a transaction is currently active.

```cpp
bool pending() const noexcept
```

**Returns:**

- `true` if transaction started but not committed
- `false` otherwise

---

#### MDB\_txn\* handle() const noexcept

Returns the raw LMDB transaction handle.

```cpp
MDB_txn* handle() const noexcept
```

**Returns:**

- Pointer to `MDB_txn`

## `dbi_t` – LMDB Database Handle Wrapper

RAII-style C++20 wrapper for `MDB_dbi` that represents a named or unnamed LMDB database inside an environment.

---

### Constructors

#### Default constructor

Constructs an empty database handle.

```cpp
dbi_t()
```

**Parameters:** None

**Behaviour:** Initializes with no open database. Must call `open()` before use.

---

#### Move constructor

Transfers ownership from another `dbi_t`.

```cpp
dbi_t(dbi_t&& other) noexcept
```

**Parameters:**

- `other`: Rvalue reference to another `dbi_t`

**Behaviour:** Moves internal database handle. Leaves `other` in a closed state.

---

### Operators

#### dbi\_t& operator=(dbi\_t&& other) noexcept

Transfers ownership from another `dbi_t`.

```cpp
dbi_t& operator=(dbi_t&& other) noexcept
```

**Parameters:**

- `other`: Rvalue reference to another `dbi_t`

**Behaviour:** Moves the database handle and opened flag.

---

### Member Functions

#### error\_t open(txn\_t& txn, std::string\_view name = {}, Flags... flags)

Opens an LMDB database within a transaction.

```cpp
template<typename... Flags>
error_t open(txn_t& txn, std::string_view name = {}, Flags... flags) noexcept
```

**Parameters:**

- `txn`: Active LMDB transaction
- `name`: Optional database name (empty string for unnamed default DB)
- `flags`: Optional LMDB database open flags

**Behaviour:** Opens or creates a database inside the transaction. Sets internal `MDB_dbi` handle.

**Returns:**

- `MDB_SUCCESS` or LMDB error code

---

#### error\_t erase(txn\_t& txn) const noexcept

Erases all key-value pairs from the database.

```cpp
error_t erase(txn_t& txn) const noexcept
```

**Parameters:**

- `txn`: Active write transaction

**Returns:**

- `MDB_SUCCESS` or LMDB error code

---

#### error\_t drop(txn\_t& txn) noexcept

Drops the database and closes its handle.

```cpp
error_t drop(txn_t& txn) noexcept
```

**Parameters:**

- `txn`: Active write transaction

**Returns:**

- `MDB_SUCCESS` or LMDB error code

---

#### bool isopen() const noexcept

Checks whether the database handle is open.

```cpp
bool isopen() const noexcept
```

**Returns:**

- `true` if open, `false` otherwise

---

#### MDB\_dbi handle() const noexcept

Returns the underlying LMDB database handle.

```cpp
MDB_dbi handle() const noexcept
```

**Returns:**

- The `MDB_dbi` handle value

---

#### error\_t stat(const txn\_t& txn, MDB\_stat& out) const noexcept

Retrieves statistics for the database.

```cpp
error_t stat(const txn_t& txn, MDB_stat& out) const noexcept
```

**Parameters:**

- `txn`: Active transaction
- `out`: Output structure to receive statistics

**Returns:**

- `MDB_SUCCESS` or LMDB error code

---

#### template\<detail::SerializableLike KeyT, ValueT, typename... Flags>

error\_t put(const txn\_t& txn, const KeyT& key, const ValueT& value, Flags... flags) const noexcept Inserts or updates a key-value pair.

```cpp
template<detail::SerializableLike KeyT, detail::SerializableLike ValueT, typename... Flags>
error_t put(const txn_t& txn, const KeyT& key, const ValueT& value, Flags... flags) const noexcept
```

**Parameters:**

- `txn`: Active write transaction
- `key`: Key to insert or update
- `value`: Value to store
- `flags`: Optional put flags

**Returns:**

- `MDB_SUCCESS` or LMDB error

---

#### template\<detail::SerializableLike KeyT, ValueT>

error\_t get(const txn\_t& txn, const KeyT& key, ValueT& out\_value) const noexcept Retrieves the value for a given key.

```cpp
template<detail::SerializableLike KeyT, detail::SerializableLike ValueT>
error_t get(const txn_t& txn, const KeyT& key, ValueT& out_value) const noexcept
```

**Parameters:**

- `txn`: Active transaction
- `key`: Key to query
- `out_value`: Reference to hold the result

**Returns:**

- `MDB_SUCCESS`, `MDB_NOTFOUND`, or LMDB error

---

#### template\<detail::SerializableLike KeyT>

error\_t del(const txn\_t& txn, const KeyT& key) const noexcept Deletes all values for a given key.

```cpp
template<detail::SerializableLike KeyT>
error_t del(const txn_t& txn, const KeyT& key) const noexcept
```

**Parameters:**

- `txn`: Active write transaction
- `key`: Key to delete

**Returns:**

- `MDB_SUCCESS`, `MDB_NOTFOUND`, or error

---

#### template\<detail::SerializableLike KeyT, ValueT>

error\_t del(const txn\_t& txn, const KeyT& key, const ValueT& value) const noexcept Deletes a specific key-value pair in DUPSORT DBs.

```cpp
template<detail::SerializableLike KeyT, detail::SerializableLike ValueT>
error_t del(const txn_t& txn, const KeyT& key, const ValueT& value) const noexcept
```

**Parameters:**

- `txn`: Active write transaction
- `key`: Key to locate
- `value`: Value to delete

**Returns:**

- `MDB_SUCCESS` or LMDB error

## `cursor_t` – LMDB Cursor Wrapper

RAII-style C++20 wrapper for `MDB_cursor*` providing low-level cursor operations for navigating key-value pairs in an LMDB database.

---

### Constructors

#### Default constructor

Constructs an empty cursor object.

```cpp
cursor_t()
```

**Parameters:** None

**Behaviour:** Initializes with no open cursor. Must call `open()` before use.

---

#### Move constructor

Transfers ownership from another `cursor_t`.

```cpp
cursor_t(cursor_t&& other) noexcept
```

**Parameters:**

- `other`: Rvalue reference to another `cursor_t`

**Behaviour:** Transfers ownership of the LMDB cursor. Clears `other`.

---

### Operators

#### cursor\_t& operator=(cursor\_t&& other) noexcept

Transfers ownership from another `cursor_t`.

```cpp
cursor_t& operator=(cursor_t&& other) noexcept
```

**Parameters:**

- `other`: Rvalue reference to another `cursor_t`

**Behaviour:** Closes current cursor, transfers pointer and context from `other`, and clears `other`.

---

### Destructor

#### \~cursor\_t()

Closes the cursor if open.

```cpp
~cursor_t()
```

**Behaviour:** Automatically calls `close()`.

---

### Member Functions

#### error\_t open(txn\_t& txn, dbi\_t& dbi)

Opens a cursor for a given transaction and database.

```cpp
error_t open(txn_t& txn, dbi_t& dbi) noexcept
```

**Parameters:**

- `txn`: Active transaction
- `dbi`: Open database handle

**Returns:**

- `MDB_SUCCESS` or error code

---

#### void close()

Closes the cursor.

```cpp
void close() noexcept
```

**Returns:** None

---

#### bool isopen() const noexcept

Checks if the cursor is open.

```cpp
bool isopen() const noexcept
```

**Returns:**

- `true` if open, otherwise `false`

---

#### MDB\_cursor\* handle() const noexcept

Returns the raw LMDB cursor pointer.

```cpp
MDB_cursor* handle() const noexcept
```

**Returns:**

- Pointer to `MDB_cursor` or `nullptr`

---

#### error\_t renew(txn\_t& txn)

Rebinds the cursor to a new read-only transaction.

```cpp
error_t renew(txn_t& txn) noexcept
```

**Parameters:**

- `txn`: Active read-only transaction

**Returns:**

- `MDB_SUCCESS` or error code

---

#### MDB\_dbi dbi() const noexcept

Returns the database handle associated with the cursor.

```cpp
MDB_dbi dbi() const noexcept
```

**Returns:**

- The `MDB_dbi` value

---

#### txn\_t\* txn() const noexcept

Returns the associated transaction pointer.

```cpp
txn_t* txn() const noexcept
```

**Returns:**

- Pointer to `txn_t` or `nullptr`

---

#### error\_t count(std::size\_t& out) const noexcept

Gets the number of duplicate values at the current key.

```cpp
error_t count(std::size_t& out) const noexcept
```

**Parameters:**

- `out`: Output variable to receive count

**Returns:**

- `MDB_SUCCESS` or error code

---

#### error\_t get(KeyT& key, ValueT& value, MDB\_cursor\_op op) const noexcept

Fetches a key-value pair using the given cursor operation.

```cpp
template<detail::SerializableLike KeyT, detail::SerializableLike ValueT>
error_t get(KeyT& key, ValueT& value, MDB_cursor_op op) const noexcept
```

**Parameters:**

- `key`: Input/output key
- `value`: Output value
- `op`: LMDB cursor operation code

**Returns:**

- `MDB_SUCCESS`, `MDB_NOTFOUND`, or error

---

#### error\_t put(const KeyT& key, const ValueT& value, Flags... flags) noexcept

Inserts a key-value pair at the current cursor position.

```cpp
template<detail::SerializableLike KeyT, detail::SerializableLike ValueT, typename... Flags>
error_t put(const KeyT& key, const ValueT& value, Flags... flags) noexcept
```

**Parameters:**

- `key`: Key to insert
- `value`: Value to insert
- `flags`: Optional LMDB put flags

**Returns:**

- `MDB_SUCCESS`, `MDB_KEYEXIST`, or error

---

#### error\_t del(const KeyT& key) noexcept

Deletes all values for the given key.

```cpp
template<detail::SerializableLike KeyT>
error_t del(const KeyT& key) noexcept
```

**Parameters:**

- `key`: Key to delete

**Returns:**

- `MDB_SUCCESS`, `MDB_NOTFOUND`, or error

---

#### error\_t del(const KeyT& key, const ValueT& value) noexcept

Deletes a specific key-value pair.

```cpp
template<detail::SerializableLike KeyT, detail::SerializableLike ValueT>
error_t del(const KeyT& key, const ValueT& value) noexcept
```

**Parameters:**

- `key`: Key to locate
- `value`: Value to match

**Returns:**

- `MDB_SUCCESS` or LMDB error

---

#### error\_t del\_all\_dups() noexcept

Deletes all duplicate values for the current key.

```cpp
error_t del_all_dups() noexcept
```

**Returns:**

- `MDB_SUCCESS` or error

## `error_t` – LMDB Error Wrapper

Lightweight wrapper for LMDB error codes. Provides an object-oriented interface for inspecting and propagating LMDB API results.

---

### Constructors

#### Default constructor

Constructs a success result.

```cpp
error_t()
```

**Behaviour:** Initializes with `MDB_SUCCESS` (no error).

---

#### Explicit constructor from error code

Wraps an LMDB integer return code.

```cpp
explicit error_t(int code) noexcept
```

**Parameters:**

- `code`: LMDB return code to wrap

**Behaviour:** Stores the code for later inspection.

---

#### Copy constructor

Constructs a copy of another `error_t` instance.

```cpp
error_t(const error_t& other)
```

**Parameters:**

- `other`: The `error_t` instance to copy

**Behaviour:** Copies the internal error code.

---

#### Move constructor

Constructs from a moved `error_t` instance.

```cpp
error_t(error_t&& other) noexcept
```

**Parameters:**

- `other`: The `error_t` instance to move from

**Behaviour:** Transfers ownership of the internal error code.

---

### Operators

#### error\_t& operator=(const error\_t& other)

Assigns from another `error_t`.

```cpp
error_t& operator=(const error_t& other)
```

**Behaviour:** Copies the error code.

---

#### error\_t& operator=(int code)

Assigns from a raw LMDB error code.

```cpp
error_t& operator=(int code) noexcept
```

**Parameters:**

- `code`: LMDB return code

**Behaviour:** Replaces the current code with a new one.

---

#### operator bool() const noexcept

Returns `true` if the error is `MDB_SUCCESS`.

```cpp
operator bool() const noexcept
```

**Returns:**

- `true` if no error, otherwise `false`

---

#### operator int() const noexcept

Returns the raw LMDB error code.

```cpp
operator int() const noexcept
```

**Returns:**

- Integer LMDB status code

---

### Member Functions

#### bool ok() const noexcept

Checks whether the result is successful.

```cpp
bool ok() const noexcept
```

**Returns:**

- `true` if code is `MDB_SUCCESS`, `false` otherwise

---

#### int code() const noexcept

Returns the raw LMDB error code.

```cpp
int code() const noexcept
```

**Returns:**

- The internal status code

---

#### std::string message() const

Returns a human-readable string for the error code.

```cpp
std::string message() const
```

**Returns:**

- LMDB error message as a string (e.g. from `mdb_strerror`)

### License

lmdbpp is licensed with the same license as LMDB, The OpenLDAP Public License. A copy of this license is available in the file LICENSE in the top-level directory of the distribution or, alternatively, at http://www.OpenLDAP.org/license.html. Please also refer to Acknowledgement section below.

### Dependency on Catch2 for Unit Tests

lmdbpp classes and methods are tested using Catch2 unit test framework. You can download Catch2 on Github using the following link: 
[Catch2 v2.x][Catch2]

[Catch2]: https://github.com/catchorg/Catch2/tree/v2.x/single_include

## LMDB Acknowledgement

### Author
Howard Chu, Symas Corporation.

### Copyright
LMDB source code Copyright 2011-2015 Howard Chu, Symas Corp. All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted only as authorized by the OpenLDAP Public License.

A copy of this license is available in the file LICENSE in the top-level directory of the distribution or, alternatively, at http://www.OpenLDAP.org/license.html.

### Derived from:
LMDB code is derived from btree.c written by Martin Hedenfalk. 
Copyright (c) 2009, 2010 Martin Hedenfalk martin@bzero.se

Permission to use, copy, modify, and distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
