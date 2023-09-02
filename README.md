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
The charts below how the performance of LMDB, represented by the MDB collumn, when compared to similar key/value store databases:

<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-read-performance-sr.png" width="700" height="300" border="10"/>
<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-read-performance-lr.png" width="700" height="300" border="10"/>
<img src="https://github.com/rmgmachado/lmdbpp/blob/main/graphs/lmdb-write-performance-lr.png" width="700" height="300" border="10"/>

## lmdbpp C++ wrapper

lmdbpp implements a simple C++ wrapper around the LMDB C API. lmdbpp exposes classes under the namespace lmdb representing the major LMDB objects, with an additional class status_t provided to handle errors returned by LMDB:

| class | Description |
|--|--|
| environment_t  | Everything starts with an environment, created by call to startup() method. Each process should have only one environment to prevent issues with locking. lmdb::environment_t class provided cleanup() method to terminate and cleanup an environment. |
| transaction_t | Once an environment is created, a transaction can be created. Every LMDB operation needs to be performed under a read_write or read_only transaction. begin() method starts a transaction, commit() commits any changes, while abort() reverts any changes |
| table_t | After a transaction is started with a call to transaction_t::begin(), a new table can be created, or an existing table can be opened. operations such as get(), put() and del() can be performed with table_t object |
| cursor_t | After a table is opened, a cursor_t object can be created to perform cursor operations such as first(), last(), next(), prior(), seek(), search() and find(), put() and del() |
| status_t | almost all calls to lmdbpp class methods return a status_t object. ok() method returns true() if the operation succeeded, while nok() returns true if the operation failed. error() method returns the error code provided ty LMDB, while message() returns an std::string with the appropriate error message |

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

### lmdb::environment_t class

lmdbpp lmdb::environment_t class wraps all the LMDB environment operations. lmdb::environment_t prevents copying, but a move constructor and operator is provided. Please note that only one environment should be created per process, to avoid issues with some OSses advisory locking. 

#### environment_t() constructor
lmdb::environment_t class declares only a default constructor. Please note copy constructor and assignment operator have both been deleted, as this object cannot be copied. On the other hand, a move constructor and a move operator has been provided to transfer ownership to another lmdb::environment_t object. lmdb::environment_t automatically invokes cleanup() to close the environment and release resources.

Example:
```C++
#include "lmdbpp.h"

lmdb::environment_t env;
```

#### environment_t::startup() method
Create or open a new LMDB environment. When the enviroment_t object is no longer needed, call to cleanup() to close the environment and release resources. The lmdb::environment_t destructor calls cleanup() automatically if the environment is still open when the object is destroyed.

```C++
#include "lmdbpp.h"

status_t startup(const std::string& path,
                 unsigned int max_tables = DEFAULT_MAXTABLES,
                 size_t mmap_size = DEFAULT_MMAPSIZE,
                 unsigned int max_readers = DEFAULT_MAXREADERS) noexcept;
```

| Parameter | In/Out | Description |
|----|:--:|----|
| path | In | The directory in which the database files reside. This directory must already exist and be writable. |
| max_tables | In | Maximum number of tables in a database |
| mmap_size | In | Set the size of the memory map to use for this environment. The size should be a multiple of the OS page size. The default is 10,485,760 bytes. Any attempt to set a size smaller than the space already consumed by the environment will be silently changed to the current size of the used space.  |
| max_readers | In | Set the maximum number of threads/reader slots for the environment. This defines the number of slots in the lock table that is used to track readers in the the environment. The default is 126. Starting a read-only transaction normally ties a lock table slot to the current thread until the environment closes or the thread exits. |

Parameter mmap_size is also the maximum size of the database. The value should be chosen as large as possible, to accommodate future growth of the database. The new size takes effect immediately for the current process but will not be persisted to any others until a write transaction has been committed by the current process. If the mapsize is increased by another process, and data has grown beyond the range of the current mapsize, transaction_t::begin() will return MDB_MAP_RESIZED error, in which case environment_t::mmap_size() method may be called with a size of zero to adopt the new size.

The default values used by startup() parameters:
```C++
constexpr unsigned int DEFAULT_MAXTABLES = 128;
constexpr unsigned int DEFAULT_MAXREADERS = 126;
constexpr size_t DEFAULT_MMAPSIZE = 10485760;
```
Example:

```C++
#include "lmdbpp.h"
#include <iostream>

lmdb::environment_t env;

void show_error(const char* method, const lmdb::status_t& status) noexcept
{
   std::cout << method << " failed with error " status.error() << ": " << status.message() << "\n";
}   

int main()
{
   lmdb::status_t status = env.startup(".\\");
   if (status.ok())
   {
        // create transaction, tables and perhaps cursors
   }
   else
   {
      show_error("startup()", status);
   }
   env.cleanup();
}
```

#### environment_t::cleanup() method
Close the environment and release the memory map.

```C++
#include "lmdbpp.h"

void cleanup() noexcept;
```

Only a single thread may call this function. All transactions, databases, and cursors must already be closed before calling this function. Attempts to use any such handles after calling this function will cause a SIGSEGV. The environment handle will be freed and must not be used again after this call. cleanup() is called automatically by lmdb::environment_t class to close the environment and release resources if a call to cleanup() was not made at the time the object is being destroyed.

#### environment_t::check() method
Check for stale entries in the reader lock table. check() returns the number of stale readers checked and cleared.

```C++
#include "lmdbpp.h"

int check() noexcept;
```
#### environment_t::flush() method
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

lmdb::environment_t env;

void show_error(const char* method, const lmdb::status_t& status) noexcept
{
   std::cout << method << " failed with error " status.error() << ": " << status.message() << "\n";
}   

int main()
{
   lmdb::status_t status = env.startup(".\\");
   if (status.ok())
   {
        if (status = env.flush(); status.nok()) show_error("flush()", status);
   }
   else
   {
      show_error("startup()", status);
   }
   env.cleanup();
}
```
#### environment_t::path() method
Return the path that was used at the startup() call.
```C++
#include "lmdbpp.h"

std::string path() const noexcept;
```
If path() is called before a successful startup() call, an empty string is returned.

#### environment_t::max_readers() method
Return the maximum number of threads/reader slots for the environment. 
```C++
#include "lmdbpp.h"

size_t max_readers() const noexcept;
```
The number of readers is set when environment is created with a call to startup() method. max_readers() doesn't return an error. If it cannot obtain the number of readers, it returns zero.

####  environment_t::max_tables() method
Return the maximum number of tables the environment can support. 
```C++
#include "lmdbpp.h"

size_t max_tables() const noexcept;
```
max_tables() doesn't return an error. If it cannot obtain the number of tables from the environment, it returns zero.

#### environment_t::mmap_size() method
Set the new size of the memory map, or return the memory map size for the environment. 
```C++
#include "lmdbpp.h"

size_t mmap_size() const noexcept;
status_t mmap_size(size_t sz) noexcept;
```
mmap_size() without arguments just return the current size of the memory map for the environment. This method doesn't return an error. If it cannot obtain the memory map size of the environment, it returns zero.
mmap_size(size_t sz), with a size_t argument, resizes the current memory map. This method returns status_t to indicate the result of the operation.

Example:
```C++
#include "lmdbpp.h"
#include <iostream>

lmdb::environment_t env;

void show_error(const char* method, const lmdb::status_t& status) noexcept
{
   std::cout << method << " failed with error " status.error() << ": " << status.message() << "\n";
}   

int main()
{
   lmdb::status_t status = env.startup(".\\");
   if (status.ok())
   {
        // set the size of the environment
        if (status = env.mmap_size(MAX_UINT32 / 4); status.nok()) show_error("mmap_size()", status);
        std::out << "mmap_size: " << env.mmap_size() << "\n";
   }
   else
   {
      show_error("startup()", status);
   }
   env.cleanup();
}
```
#### environment_t::max_keysize() method
Return the maximum size of keys and data we can write to an LMDB key/value pair.
```C++
#include "lmdbpp.h"

size_t max_keysize() const noexcept;
```
Depends on the compile-time constant MDB_MAXKEYSIZE. Default is 511.

#### environment_t::handle() method
Return the LMDB environment handle pointer.
```C++
#include "lmdbpp.h"

MDB_env* handle() noexcept;
```
The LMDB environment pointer is needed when instanciating transaction_t and table_t objects.

### lmdb::transaction_t class
mdbpp lmdb::transaction_t class wraps all the LMDB transaction operations. lmdb::transaction_t prevents copying, but a move constructor and move operator are provided to transfer ownwership of a transaction handle. Transactions may be read-write or read-only. A transaction must only be used by one thread at a time. Transactions are always required, even for read-only access. The transaction provides a consistent view of the data. transaction_t desctructor will automatically invoke a transaction_t::abort() if a transaction is still active at the time the transaction object is being destroyed.

To actually get anything done, a transaction must be committed using transaction_t::commit(). Alternatively, all of a transaction's operations can be discarded using transaction_t::abort().

For read-only transactions, obviously there is nothing to commit to storage. The transaction still must eventually be aborted to close any database handle(s) opened in it, or committed to keep the database handles around for reuse in new transactions. In addition, as long as a transaction is open, a consistent view of the database is kept alive, which requires storage. A read-only transaction that no longer requires this consistent view should be terminated (committed or aborted) when the view is no longer needed.

There can be multiple simultaneously active read-only transactions but only one that can write. Once a single read-write transaction is opened, all further attempts to begin one will block until the first one is committed or aborted. This has no effect on read-only transactions, however, and they may continue to be opened at any time.

#### transaction_t() constructor
Create a transaction_t object.
```C++
#include "lmdbpph.h"

transaction_t(environment_t& env) noexcept;
```
Normally the environment_t object passed to the constructor must have been initialized by environment_t::startup() call. On the other hand, the environment_t object passed to transaction_t constructor must call environment_t::startup() before a transaction is initiated by calling transaction_t::begin() method. 

#### transaction_t::begin() method
Create a transaction for use with the environment. 

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

lmdb::environment_t env;

void show_error(const char* method, const lmdb::status_t& status) noexcept
{
   std::cout << method << " failed with error " status.error() << ": " << status.message() << "\n";
}   

int main()
{
   lmdb::status_t status = env.startup(".\\");
   if (status.ok())
   {
        transaction_t txn(transaction_type_t::read_only);
        // open or create table(s) or a cursor(s)
   }
   else
   {
      show_error("startup()", status);
   }
   env.cleanup();
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
Return the LMDB environment handle pointer.
```C++
#include "lmdbpp.h"

MDB_env* handle() noexcept;
```
The LMDB environment pointer is needed when instanciating transaction_t and table_t objects.

#### transaction_t::environment() method
Return the environment object associated with this transaction.
```C++
#include "lmdbpp.h"

environment_t& environment() noexcept;
```

### lmdb::keyvalue_t type
The keyvalue_t type is a typedef of a std::pair representing a key type as the first element of the pair, the value type as the second element of the pair. Using std:pair is a convenient way of representing the data stored in LMDB database.

```C++
template <typename KEY, typename VALUE>
using keyvalue_base_t = std::pair<KEY, VALUE>;

using keyvalue_t = keyvalue_base_t<std::string, std::string>;
```

#### make_keyvalue() function
lmdb::make_keyvalue() function provides a convenient way to build a std::pair containing a key/value pair for lmdbpp.

```C++
#include "lmdbpp.h"

template <typename KEY, typename VALUE>
inline auto make_keyvalue(const KEY& key, const VALUE& value) noexcept
{
  return std::make_pair(key, value);
}
```
#### get_key() function
lmdb::get_key() returns the key element of a key/value pair.

```C++
#include "lmdbpp.h"

template <typename KEY, typename VALUE>
inline auto get_key(const keyvalue_base_t<KEY, VALUE>& kv) noexcept
{
  return kv.first;
}
```

#### get_value() function
lmdb::get_value() returns the value element of a key/value pair.

```C++
#include "lmdbpp.h"

template <typename KEY, typename VALUE>
inline auto get_value(const keyvalue_base_t<KEY, VALUE>& kv) noexcept
{
  return kv.second;
}
```

#### put_key() function
lmdb::put_key updates the key element of a key/value pair

```C++
#include "lmdbpp.h"

template <typename KEY, typename VALUE>
inline void put_key(keyvalue_base_t<KEY, VALUE>& kv, const KEY& key) noexcept
{
  kv.first = key;
}
```
#### put_value() function

```C++
#include "lmdbpp.h"

template <typename KEY, typename VALUE>
inline void put_value(keyvalue_base_t<KEY, VALUE>& kv, const VALUE& value) noexcept
{
  kv.second = value;
}
```

### lmdb::table_t class

#### table_t() constructor

#### table_t::create() method

#### table_t::open() method

#### table_t::close() method

#### table_t::drop() method

#### table_t::key_value_t type

#### table_t::get() method

#### table_t::put() method

#### table_t::del() method

#### table_t::entries() method

#### table_t::handle() method

#### table_t::environment() method

### lmdb::cursor_t class

### lmdb::status_t class

####

### License

lmdbpp is licensed with the same license as LMDB, The OpenLDAP Public License. A copy of this license is available in the file LICENSE in the top-level directory of the distribution or, alternatively, at http://www.OpenLDAP.org/license.html. Please also refer to Acknowledgement section below.

### Dependency on Catch2 for Unit Tests

lmdbpp classes and methods are tested using Catch2 unit test framework. You can download Catch2 on Github using the following link: 
[Catch2 v2.x][Catch2]

[Catch2]: https://github.com/catchorg/Catch2/tree/v2.x/single_include

## Caveats using LMDB

* A broken lock file can cause sync issues. Stale reader transactions left behind by an aborted program cause further writes to grow the database quickly, and stale locks can block further operations. To fix this issue check for stale readers periodically using environment_t::check() method.
* Only the database file owner should normally use the database on BSD systems.
* A thread can only use one transaction at a time. Please see LMDB documentation for more details
* Use environment_t in the process which started it, without fork()ing
* Do not open a LMDB database twice in the same process. It breaks flock() advisory locking.
* Avoid long-lived transactions. Read transactions prevent reuse of pages freeed by newer write transactions, thus the database can grow quickly. Write transactions prevent other write transactions, since writes are serialized.
* Avoid suspending a process with active transactions. These would then be "long-lived" as aboce.
* Avoid aborting a process with an active transaction. These would be "long-lived" as above until an environment_t::check() is called for cleanup stale readers.
* Close the environment once in a while, so the lockfile can get reset.
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
