# lmdb++
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
* Crash-proff, no recovery needed after restart
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

## lmdbpp C++ wrapper project

lmdbpp implements a simple C++ wrapper around the LMDB C API. lmdbpp exposes classes classes representing the major LMDB objects, with an additional class status_t provided to handle errors returned by LMDB:

| class | Description |
|--|--|
|  |  |

The following LMDB features are not yet implemented by lmdbpp wrapper:
* Duplicate keys - All keys in a key/pair value are unique
* No nested transactions

