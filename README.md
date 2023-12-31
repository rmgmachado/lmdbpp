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

### LMDB Performance
The charts below how the performance of LMDB, represented by the MDB collumn, when compared to similar key/value store databases. The charts shown below extracted from Howard Chu presentations and from Syma Corporation's [In-Memory Microbenchmark](http://www.lmdb.tech/bench/inmem/) and [Database Microbenchmarks](http://www.lmdb.tech/bench/microbench/):

<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-read-performance-sr.png" width="700" height="300" border="10"/>
<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-read-performance-lr.png" width="700" height="300" border="10"/>
<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-write-performance-lr.png" width="700" height="300" border="10"/>

## lmdbpp C++ wrapper

lmdbpp implements a simple C++ wrapper around the LMDB C API. lmdbpp exposes classes under the namespace lmdb representing the major LMDB objects, with an additional class status_t provided to handle errors returned by LMDB:

| class | Description |
|--|--|
| lmdb::database_t  | Everything starts with a database, created by call to initialize() method. Each process should have only one database to prevent issues with locking. Call lmdb::database_t class cleanup() method to close and cleanup a database. This lmdb::database_t wraps the operations performed by LMDB environment|
| lmdb::transaction_t | Once a database is started, a transaction object can be created. Every LMDB operation needs to be performed under a read_write or read_only transaction. begin() method starts a transaction, commit() commits any changes, while abort() reverts any changes |
| lmdb::store_t | This is equivalent to a table in a SQL database, and perform operations such as get(), put() and del() can be performed with store_t object. In LMDB this object is usually referenced as a DBI |
| lmdb::cursor_t | After a store is opened or created, a cursor_t object can be created to perform cursor operations such as first(), last(), next(), prior(), seek(), search() and find(), put() and del() |
| lmdb::status_t | almost all calls to lmdbpp class methods return a status_t object. ok() method returns true if the operation succeeded, while nok() returns true if the operation failed. error() method returns the error code provided ty LMDB, while message() returns an std::string object with the appropriate error message |

### lmdbpp limitations
The following LMDB features are not yet implemented by lmdbpp wrapper:
* No duplicate keys - all keys in a key/pair value are unique
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

### lmdb::database_t class

lmdbpp lmdb::database_t class wraps all the LMDB environment operations. lmdb::database_t prevents copying, but a move constructor and operator is provided. Please note that only one environment should be created per process, to avoid issues with some OSses advisory locking. 

#### environment_t() constructor
lmdb::environment_t class declares only a default constructor. Please note copy constructor and assignment operator have both been deleted, as this object cannot be copied. On the other hand, a move constructor and a move operator has been provided to transfer ownership to another lmdb::environment_t object. lmdb::environment_t automatically invokes cleanup() to close the environment and release resources.

Example:
```C++
#include "lmdbpp.h"

lmdb::environment_t env;
```

#### environment_t::initialize() method
Initialize a LMDB database. When the database_t object is no longer needed, call to cleanup() to close the database and release resources. The lmdb::database_t destructor calls cleanup() automatically if the environment is still open when the object is destroyed.

```C++
#include "lmdbpp.h"

status_t initialize(const std::string& path,
                 unsigned int max_stores = DEFAULT_MAXSTORES,
                 size_t mmap_size = DEFAULT_MMAPSIZE,
                 unsigned int max_readers = DEFAULT_MAXREADERS) noexcept;
```

| Parameter | In/Out | Description |
|----|:--:|----|
| path | In | The directory in which the database files reside. This directory must already exist and be writable. |
| max_stores | In | Maximum number of stores in a database |
| mmap_size | In | Set the size of the memory map to use for this environment. The size should be a multiple of the OS page size. The default is 10,485,760 bytes. Any attempt to set a size smaller than the space already consumed by the environment will be silently changed to the current size of the used space.  |
| max_readers | In | Set the maximum number of threads/reader slots for the environment. This defines the number of slots in the lock table that is used to track readers in the the environment. The default is 126. Starting a read-only transaction normally ties a lock table slot to the current thread until the environment closes or the thread exits. |

Parameter mmap_size is also the maximum size of the database. The value should be chosen as large as possible, to accommodate future growth of the database. The new size takes effect immediately for the current process but will not be persisted to any others until a write transaction has been committed by the current process. If the mapsize is increased by another process, and data has grown beyond the range of the current mapsize, transaction_t::begin() will return MDB_MAP_RESIZED error, in which case database_t::mmap_size() method may be called with a size of zero to adopt the new size.

For most situations the default values of the parameters are sufficient. The default values used by startup() parameters:
```C++
constexpr unsigned int DEFAULT_MAXSTORES = 128;
constexpr unsigned int DEFAULT_MAXREADERS = 126;
constexpr size_t DEFAULT_MMAPSIZE = 10485760;
```
Example:

```C++
#include "lmdbpp.h"
#include <iostream>

lmdb::database_t db;

void show_error(const lmdb::status_t & status, const char* method) noexcept
{
   std::cout << method << " failed with error " << status.error() << ": " << status.message() << "\n";
}

int main_f()
{
   lmdb::status_t status = db.initialize(".\\");
   if (status.ok())
   {
      // create transaction, stores and perhaps cursors
   }
   else
   {
      show_error(status, __func__);
   }
   db.cleanup();
   return status.error();
}
```

#### database_t::cleanup() method
Close the database and release the memory map.

```C++
#include "lmdbpp.h"

void cleanup() noexcept;
```

Only a single thread may call this function. All transactions, databases, and cursors must already be closed before calling this function. Attempts to use any such handles after calling this function will cause a SIGSEGV. The database handle will be freed and must not be used again after this call. cleanup() is called automatically by destructor to close the database and release resources if a call to cleanup() was not made at the time the object is being destroyed.

#### database_t::check() method
Check for stale entries in the reader lock table. check() returns the number of stale readers checked and cleared.

```C++
#include "lmdbpp.h"

int check() noexcept;
```
#### database_t::flush() method
Flush the data buffers to disk. 

```C++
#include "lmdbpp.h"

status_t flush() noexcept;
```
Data is always written to disk when a transaction is commited, but the operating system may keep it buffered. LMDB always flushes the OS buffers upon commit as well. 

Example:
```C++
#include "lmdbpp.h"
#include <iostream>

lmdb::database_t db;

void show_error(const lmdb::status_t& status, const char* method) noexcept
{
   std::cout << method << " failed with error " status.error() << ": " << status.message() << "\n";
}   

int main()
{
   lmdb::status_t status = db.initialize(".\\");
   if (status.ok())
   {
        if (status = db.flush(); status.nok()) show_error("flush()", status);
   }
   else
   {
      show_error("status, __func__);
   }
   env.cleanup();
}
```
#### database_t::path() method
Return the path that was used at the startup() call.
```C++
#include "lmdbpp.h"

std::string path() const noexcept;
```
If path() is called before a successful startup() call, an empty string is returned.

#### database_t::max_readers() method
Return the maximum number of threads/reader slots for the database. 
```C++
#include "lmdbpp.h"

size_t max_readers() const noexcept;
```
The number of readers is set when a database is initialized. max_readers() doesn't return an error. If it cannot obtain the number of readers, it returns zero.

####  database_t::max_stores() method
Return the maximum number of stores the database can support. 
```C++
#include "lmdbpp.h"

size_t max_stores() const noexcept;
```
max_stores() doesn't return an error. If it cannot obtain the number of stores from the database, it returns zero.

#### database_t::mmap_size() method
Return the memory map size for the database. 
```C++
#include "lmdbpp.h"

size_t mmap_size() const noexcept;
```
mmap_size() returns the current size of the memory map for the database. This method doesn't return an error. If it cannot obtain the memory map size of the database, it returns zero.

#### database_t::max_keysize() method
Return the maximum size of keys and data we can write to an LMDB key/value pair.
```C++
#include "lmdbpp.h"

size_t max_keysize() const noexcept;
```
Depends on the compile-time constant MDB_MAXKEYSIZE. Default is 511.

#### database_t::handle() method
Return the LMDB database handle pointer.
```C++
#include "lmdbpp.h"

MDB_env* handle() noexcept;
```
The LMDB database handle pointer is needed when instanciating transaction_t and store_t objects.

### lmdb::transaction_t class
mdbpp lmdb::transaction_t class wraps all the LMDB transaction operations. lmdb::transaction_t prevents copying, but a move constructor and move operator are provided to transfer ownwership of a transaction handle. Transactions may be read-write or read-only. A transaction must only be used by one thread at a time. Transactions are always required, even for read-only access. The transaction provides a consistent view of the data. transaction_t desctructor will automatically invoke a transaction_t::abort() if a transaction is still active at the time the transaction object is being destroyed.

To actually get anything done, a transaction must be committed using transaction_t::commit(). Alternatively, all of a transaction's operations can be discarded using transaction_t::abort().

For read-only transactions, obviously there is nothing to commit to storage. The transaction still must eventually be aborted to close any database handle(s) opened in it, or committed to keep the database handles around for reuse in new transactions. In addition, as long as a transaction is open, a consistent view of the database is kept alive, which requires storage. A read-only transaction that no longer requires this consistent view should be terminated (committed or aborted) when the view is no longer needed.

There can be multiple simultaneously active read-only transactions but only one that can write. Once a single read-write transaction is opened, all further attempts to begin one will block until the first one is committed or aborted. This has no effect on read-only transactions, however, and they may continue to be opened at any time.

#### transaction_t() constructor
Create a transaction_t object.
```C++
#include "lmdbpph.h"

transaction_t(database_t& env) noexcept;
```
Normally the database_t object passed to the constructor must have been initialized by database_t::initialize() call. On the other hand, the database_t object passed to transaction_t constructor must call database_t::startup() before a transaction is initiated by calling transaction_t::begin() method. 

#### transaction_t::begin() method
Create a transaction. 

```C++
#include "lmdbpph.h"

status_t begin(transaction_type_t type) noexcept;
```

A transaction started with transaction_t::begin() must be terminated with a transaction_t::commit() to save the changes, or transaction_t::abort() to discard any changes. A transaction and its cursors must only be used by a single thread, and s thread may only have a single transaction at a time. Please note, cursors may not span transactions. 

transaction_t::begin() accepts one argument specifying the transaction type"
* transaction_type_t::read_only - begin a read only transaction
* transaction_type_t::read_write - begin a read write transaction

If transaction_type_t::none is passed as parameter for transaction_t::begin() status_t returns a **MDB_INVALID_TRANSACTION_TYPE** error.

Example:

```C++
#include "lmdbpp.h"
#include <iostream>

lmdb::database_t db;

void show_error(const lmdb::status_t& status, const char* method) noexcept
{
   std::cout << method << " failed with error " << status.error() << ": " << status.message() << "\n";
}

int main()
{
   lmdb::status_t status = db.initialize(".\\");
   if (status.ok())
   {
      lmdb::transaction_t txn(db);
      // open or create store(s) or a cursor(s)
      status = txn.begin(transaction_type_t::read_only);
      if (status.nok())
      {
         show_error(status, __func__);
      }
   }
   else
   {
      show_error(status, __func__);
   }
   db.cleanup();
}
```

#### transaction_t::commit() method
Commit any changes to data occurred after a transaction started with transaction_t::begin().

```C++
#include "lmdbpph.h"

status_t commit() noexcept;
```

#### transaction_t::abort() method
Discard any changes to data occurred after a transaction started with transaction_t::begin();

```C++
#include "lmdbpph.h"

status_t abort() noexcept;
```

#### transaction_t::started() method
Indicate if a transaction is active, i.e. was started with a call to begin().

```C++
#include "lmdbpph.h"

bool started() const noexcept;
```

#### transaction_t::type() method
Return the transaction type of the transaction object.

```C++
#include "lmdbpph.h"

transaction_type_t type() const noexcept;
```
The possible return types are:
* transaction_type_t::read_only - transaction is active and it's a read only transaction
* transaction_type_t::read_write - transaction is active and it's a read write transaction
* transaction_type_t::none - transaction is not active

#### transaction_t::handle() method
Return the LMDB database handle pointer.
```C++
#include "lmdbpp.h"

MDB_env* handle() noexcept;
```
The LMDB database handle pointer is needed when instanciating transaction_t and store_t objects.

#### transaction_t::database() method
Return the database object associated with this transaction.
```C++
#include "lmdbpp.h"

database_t& database() noexcept;
```

### lmdb::store_t class
mdbpp lmdb::store_t class wraps all the LMDB operations operations. All store operations must be done under transaction control, either a read-only or a read-write transaction. lmdb::transaction_t prevents copying, but a move constructor and move operator are provided to transfer ownwership of a transaction handle. 

#### store_t() constructor
Construct a lmdb::store_t object and the only parameter is a lmdb::database_t object.

```C++
#include "lmdbpp.h"

template <typename KEY, typename VALUE>
store_t(database_t& env) noexcept;
```
database_t object passed to the constructor must have been initialized by databa_t::initialize() call. 

#### store_t::create() method
Open an existing store, or create a new store if one doesn't exist in the database.
```C++
#include "lmdbpp.h"

status_t create(transaction_t& txn, const std::string& name) noexcept;
```
The transaction object passed as parameter must have been started with a read-write transaction_t::begin() method. The name of the store must not contain a directory path, as the store will be created or opened within the database whose path was specified in database_t::initialize() method. The transaction must be commited for store create to take effect.

Example:
```C++
#include "lmdbpp.h"
#include <iostream>

lmdb::database_t db;

void show_error(const lmdb::status_t& status, const char* method) noexcept
{
   std::cout << method << " failed with error " << status.error() << ": " << status.message() << "\n";
}

int main()
{
   lmdb::status_t status = db.initialize(".\\");
   if (status.nok())
   {
      show_error(status, __func__);
         return status.error();
   }
   lmdb::transaction_t txn(db);
   if (status = txn.begin(transaction_type_t::read_write); status.nok())
   {
      show_error(status, __func__);
      return status.error(); // txn and env destructors will do the cleanup
   }
   lmdb::store_t tbl(db);
   status = tbl.create(txn, "test.mdb"); 
   if (status.nok())
   {
      show_error(status, __func__);
      return status.error();
   }
   status = txn.commit();
   if (status.nok())
   {
      show_error(status, __func__);
      return status.error();
   }
   // perform other store_t or cursor_t operations
   db.cleanup();
   return 0;
}
```

#### store_t::open() method
Open an existing store. If store doesn't exist in the database, an error is returned.
```C++
#include "lmdbpp.h"

status_t open(transaction_t& txn, const std::string& name) noexcept;
```
The transaction object passed as parameter must have been started with a read-write transaction_t::begin() method. The name of the store must be the same name used with a previous store_t::create() call.

#### store_t::close() method
Close a store and release resources.

```C++
#include "lmdbpp.h"

status_t close(transaction_t&) noexcept;
```

#### store_t::drop() method
Delete a store from the database.
```C++
#include "lmdbpp.h"

status_t drop(transaction_t& txn) noexcept;
```
A store must be open before you call drop() method, and a read-write transaction must be in effect.

#### store_t::get() method
Retrieve a key/value pair from the store. 

```C++
#include "lmdbpp.h"

status_t get(transaction_t& txn, key_const_reference target_key, key_reference key, value_reference value) noexcept;
status_t get(transaction_t& txn, key_const_reference target_key, keyvalue_t& kv) noexcept;
```
The store must be open and you must hav an active read-only or read-write transaction. target_key parameter indicates the key of the key/value pair to be retrieved. store_t::get() returns status with MDB_NOTFOUND error if target_key not found.

#### store_t::put() method
Insert or update a key/value pair in the store.

```C++
#include "lmdbpp.h"

status_t put(transaction_t& txn, key_const_reference key, value_const_reference value) noexcept;
status_t put(transaction_t& txn, keyvalue_t& kv) noexcept;
```
The store must be open and you must have an active read-write transaction.

#### store_t::del() method
Delete a key/value pair from the store.

```C++
#include "lmdbpp.h"

status_t del(transaction_t& txn, key_const_reference key, value_const_reference value) noexcept;
status_t del(transaction_t& txn, const keyvalue_t& kv) noexcept;
```
The store must be open and you must have an active read-write transaction.

#### store_t::entries() method
Retrieve the number of active key/pair entries in the store.

```C++
#include "lmdbpp.h"

size_t entries(transaction_t& txn) noexcept;
```
The store must be open and you must pass an active read-only or read-write transaction as parameter.

#### store_t::name() method
Retrieve the name of the store.

```C++
#include "lmdbpp.h"

std::string name() const noexcept;
```
#### store_t::handle() method
Retrieve the LMDB store pointer handle.

```C++
#include "lmdbpp.h"

MDB_dbi handle() const noexcept;
```
#### store_t::database() method
Retrieve the database object associated with this store.

```C++
#include "lmdbpp.h"

database_t& database() noexcept;
```

### lmdb::cursor_t class
A cursor_t object is associated with a specific transaction_t and store_t objects. A cursor_t cannot be used any more when its parent store_t object has been closed, nor when its transaction has ended.A cursor in a read-write transaction can be closed before its transaction ends, and will otherwise be closed when its transaction ends. A cursor in a read-only transaction must be closed explicitly, before or after its transaction ends. A cursor is used to provide multiple and independent views into a store_t object. cursor_t objects cannot be copied or assigned, but can be moved via a move constructor and a move assignment operator.

#### cursor_t constructor
Construct a new cursor_t object.

```C++
#include "lmdbpp.h"

cursor_t(store_t& table) noexcept;
cursor_t(transaction_t& txn, store_t table);
```
The constructor that takes just one store_t argument creates a cursor_t object, which needs to be opened by a call to cursor_t::open() method. On the other hand, the cursor_t constructor that take a transaction_t and store_t objects will create a new cursor_t object and attempts to open the cursor. This two argument constructor will throw an exception error_t if the open operation fails.

#### cursor_t::open() method
Open a new cursor.

```C++
#include "lmdbpp.h"

status_t open(transaction_t& txn) noexcept;
```
#### cursor_t::close() method
Close a cursor.

```C++
#include "lmdbpp.h"

status_t close() noexcept;
```
#### cursor_t::current() method
Return key-value data pair at current cursor position. The cursor position is not changed.

```C++
#include "lmdbpp.h"

status_t current(key_reference key, value_reference value) noexcept;
status_t current(key_reference key) noexcept;
```
The current method return the key-value data pair at current cursor position, while the sing argument cursor_t::current() method retrieves only the key at current cursor position.

#### cursor_t::first() method
Position cursor at first key-value data pair in a store.

```C++
#include "lmdbpp.h"

status_t first(key_reference key, value_reference value) noexcept;
status_t first(key_reference key) noexcept;
status_t first() noexcept;
```
If the store contains no records first() methods will return status_t with MDB_NOTFOUND error.
#### cursor_t::last() method
Position the cursor at the last key-value data pair in a store.
```C++
#include "lmdbpp.h"

status_t last(key_reference key, value_reference value) noexcept;
status_t last(key_reference key) noexcept;
status_t last() noexcept;
```
If the store contains no records last() methods will return status_t with MDB_NOTFOUND error.

#### cursor_t::next() method
Position the cursor at the next key-value data pair in a store.
```C++
#include "lmdbpp.h"

status_t last(key_reference key, value_reference value) noexcept;
status_t last(key_reference key) noexcept;
status_t last() noexcept;
```
If the cursor is already at the last record of a store, next() methods will return status_t with MDB_NOTFOUND error.

#### cursor_t::prior() method
Position the cursor at the previous key-value data pair in a store.
```C++
#include "lmdbpp.h"

status_t prior(key_reference key, value_reference value) noexcept;
status_t prior(key_reference key) noexcept;
status_t prior() noexcept;
```
If the cursor is already at the first record of a store, prior() methods will return status_t with MDB_NOTFOUND error.

#### cursor_t::seek() method
Position the cursor at the specified target key.

```C++
#include "lmdbpp.h"

status_t seek(key_const_reference target_key) noexcept;
status_t seek(key_const_reference target_key, value_reference) noexcept;
```
Position the cursor at the specified target_key. If the target_key is not present in the store_t object, seek() methods return status_t with MDB_NOTFOUND error.

#### cursor_t::find() method
Position the cursor at specified target_key and retrieve the key and the value.
```C++
#include "lmdbpp.h"

status_t find(key_const_reference target_key, key_reference key, value_reference) noexcept;
```
Position the cursor at the specified target_key. If the target_key is not present in the store_t object, find() methods return status_t with MDB_NOTFOUND error.

#### cursor_t::search() method
Position the cursor at first key greater than or equal to the target_key, and retrieve both key and value at the cursor postion.
```C++
#include "lmdbpp.h"

status_t search(key_const_reference target_key, key_reference key, value_reference) noexcept;
```
If the target_key is not present in the store_t object, find() methods return status_t with MDB_NOTFOUND error.
#### cursor_t::put() method
Add new record, or update existing record in store.
```C++
#include "lmdbpp.h"

status_t put(key_const_reference key, key_reference key, value_const_reference) noexcept;
```
put() method can only be called if the cursor_t object was opened with a read-write transaction.

#### cursor_t::del() method
Remove an existing record from store at the current cursor position.
```C++
#include "lmdbpp.h"

status_t del() noexcept;
```
del() method can only be called if the cursor_t object was opened with a read-write transaction.

#### cursor_t::database() method
Retrieve the database_t object for this cursor.
```C++
#include "lmdbpp.h"

database_t& database() noexcept;
```

#### cursor_t::store() method
Retrieve the store_t object for this cursor.
```C++
#include "lmdbpp.h"

store_t& store() noexcept;
```
#### cursor_t::handle() method
Retrieve the LMDB native handle for this cursor object.
```C++
#include "lmdbpp.h"

MDB_cursor* handle() noexcept;
```

### lmdb::status_t class
All methods in lmdbpp return a status_t object to indicate success or failure of a LMDB operation. status_t::ok() method returns true if the operation succeeded, false otherwise. status_t::nok() returns true on failure.

#### status_t::operator==() and status_t::operator!=() 
These operators can be use to compare status_t objects.
```C++
#include "lmdbpp.h"

bool operator==(const status_t& other) const noexcept;
bool operator!=(const status_t& other) const noexcept;
```

#### status_t::ok() method
Return true if LMDB operation succeeded.
```C++
#include "lmdbpp.h"

bool ok() const noexcept;
```

#### status_t::nok() method
Return true if LMDB operation failed.
```C++
#include "lmdbpp.h"

bool nok() const noexcept;
```

#### status_t::error() method
Return the LMDB error code if an operation failed.
```C++
#include "lmdbpp.h"

int error() const noexcept;
```

#### status_t::message() method
Return std::string with error message if LMDB operation failed.
Return true if LMDB operation succeeded.
```C++
#include "lmdbpp.h"

std::string message() const noexcept;
```

### License

lmdbpp is licensed with the same license as LMDB, The OpenLDAP Public License. A copy of this license is available in the file LICENSE in the top-level directory of the distribution or, alternatively, at http://www.OpenLDAP.org/license.html. Please also refer to Acknowledgement section below.

### Dependency on Catch2 for Unit Tests

lmdbpp classes and methods are tested using Catch2 unit test framework. You can download Catch2 on Github using the following link: 
[Catch2 v2.x][Catch2]

[Catch2]: https://github.com/catchorg/Catch2/tree/v2.x/single_include

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
