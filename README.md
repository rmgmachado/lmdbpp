# lmdbpp
C++ warpper for Symas LMDB key/value pair database

LMDB is a B+tree-based database management library modeled loosely on the BerkeleyDB API, but much simplified. The entire database is exposed in a memory map, and all data fetches return data directly from the mapped memory, so no malloc's or memcpy's occur during data fetches. As such, the library is extremely simple because it requires no page caching layer of its own, and it is extremely high performance and memory-efficient. It is also fully transactional with full ACID semantics, and when the memory map is read-only, the database integrity cannot be corrupted by stray pointer writes from application code.

The library is fully thread-aware and supports concurrent read/write access from multiple processes and threads. Data pages use a copy-on- write strategy so no active data pages are ever overwritten, which also provides resistance to corruption and eliminates the need of any special recovery procedures after a system crash. Writes are fully serialized; only one write transaction may be active at a time, which guarantees that writers can never deadlock. The database structure is multi-versioned so readers run with no locks; writers cannot block readers, and readers don't block writers.

Unlike other well-known database mechanisms which use either write-ahead transaction logs or append-only data writes, LMDB requires no maintenance during operation. Both write-ahead loggers and append-only databases require periodic checkpointing and/or compaction of their log or database files otherwise they grow without bound. LMDB tracks free pages within the database and re-uses them for new write operations, so the database size does not grow without bound in normal use.

References: [LMDB source code Github][lmdb source], [LMDB documentation link][lmdb doc], [LMDB Getting Started Guide][lmdb guide], [LMDB API Reference][lmdb reference]

[lmdb source]: https://github.com/LMDB/lmdb
[lmdb doc]: http://www.lmdb.tech/doc/index.html
[lmdb guide]: http://www.lmdb.tech/doc/starting.html
[lmdb reference]: http://www.lmdb.tech/doc/group__mdb.html

## LMDB Major Features

* Key/Value store using B+trees
* Fully transactional, ACID compliant
* MVCC, readers never block
* Uses memory-mapped files, needs no tuning
* Crash-proof, no recovery needed after restart
* Highly optimized, extremely compact - C code under 40kb object code
* Runs on most modern OSs = Linux, Android, MacOSX, *BSD, Windows, etc

### Concurrency Support

* Both multi-process and multi-thread
* Single Writer + N Readers
   * Writers don't block readers
   * Readers don't block writers
   * Reads scale perfectly linearly with available CPU cores
* Nested transactions
* Batched writes

## lmdbpp C++ wrapper

lmdbpp implements a simple C++ wrapper around the LMDB C API. lmdbpp exposes classes under the namespace lmdb representing the major LMDB objects, with an additional class status_t provided to handle errors returned by LMDB:

| class | Description |
|--|--|
| environment_t  | Everything starts with an environment, created by call to startup() method. Each process should have only one environment to prevent issues with locking. lmdb::environment_t class provided cleanup() method to terminate and cleanup an environment. |
| transaction_t | Once an environment is created, a transaction can be created. Every LMDB operation needs to be performed under a read_write or read_only transaction. begin() method starts a transaction, commit() commits any changes, while abort() reverts any changes |
| table_t | After a transaction is started with a call to transaction_t::begin(), a new table can be created, or an existing table can be opened. operations such as get(), put() and del() can be performed with table_t object |
| cursor_t | After a table is opened, a cursor_t object can be created to perform cursor operations such as first(), last(), next(), prior(), seek(), search() and find(), put() and del() |
| status_t | almost all calls to lmdbpp class methods return a status_t object. ok() method returns true() if the operation succeeded, while nok() returns true if the operation failed. error() method returns the error code provided ty LMDB, while message() returns an std::string with the appropriate error message |

The following LMDB features are not yet implemented by lmdbpp wrapper:
* No duplicate keys - all keys in a key/pair value are unique
* No nested transactions

### lmdbpp project files

| File | Description |
|--|--|
| lmdbpp.h | wrapper for LMDB API |
| lmdbpp-test.cpp | Catch2 unit test for lmdbpp code |
| lmdb.h | lmdb header file |
| mdb.c | lmdb C source code |
| midl.h | header file used internally by lmdb C source code |
| midl.c | C source code used internally by lmdb |

### lmdb::environment_t class

lmdbpp lmdb::environment_t class wraps all the LMDB environment operations. lmdb::environment_t provents copying, but a move constructor and operator is provided. Please note that only one environment should be created per process, to avoid issues with some OSses advisory locking. 

#### constructor
lmdb::environment_t class declares only a default constructor. Please note copy constructor and assignment operator have both been deleted, as this object cannot be copied. On the other hand a move constructor and a move operator has been provided to transfer ownership to another lmdb::environment_t object. Example:

<samp>
#include "lmdbpp.h"

lmdb::environment_t env;
<samp>

#### startup

#### cleanup

#### check

#### flush

#### path

#### max_readers

####  max_tables

#### mmap_size

#### max_keysize

#### handle

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

## Acknowledgement

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
