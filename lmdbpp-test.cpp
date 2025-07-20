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
#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <vector>
#include <iostream>
#include "lmdbpp.h"

TEST_CASE("Current directory", "[curdir]")
{
   std::cerr << "Current directory: " << std::filesystem::current_path() << "\n";
}

TEST_CASE("lmdbpp.h env_t class tests", "[env_t]")
{
   // This test suite checks the lifecycle and basic operations of the lmdb::env_t class.
   SECTION("Test enviroment_t default constructor and destructor")
   {
      lmdb::env_t env;
      REQUIRE(env.handle() != nullptr);
      REQUIRE(env.lasterror());
      REQUIRE(env.maxdbs() == lmdb::DEFAULT_MAXDBS);
      REQUIRE(env.maxreaders() == lmdb::DEFAULT_MAXREADERS);
      REQUIRE(env.mmapsize() == lmdb::DEFAULT_MMAPSIZE);
      REQUIRE(env.mode() == lmdb::DEFAULT_MODE);
      REQUIRE(env.maxkeysize() == 511);
      REQUIRE(env.path().empty());
      REQUIRE(env.check() == -1);
      REQUIRE(env.flush() == EINVAL);
      REQUIRE((env.maxdbs(100) && env.maxdbs() == 100));
      REQUIRE((env.maxreaders(101) && env.maxreaders() == 101));
      REQUIRE((env.mmapsize(1024 * 1024) && env.mmapsize() == 1024 * 1024));
      REQUIRE((env.mode(0600) && env.mode() == 0600));
   }

   SECTION("test env_t move constructor")
   {
      lmdb::env_t srcenv;
      REQUIRE(srcenv.lasterror());
      REQUIRE((srcenv.maxdbs(100) && srcenv.maxdbs() == 100));
      REQUIRE((srcenv.maxreaders(101) && srcenv.maxreaders() == 101));
      REQUIRE((srcenv.mmapsize(1024 * 1024) && srcenv.mmapsize() == 1024 * 1024));
      REQUIRE((srcenv.mode(0600) && srcenv.mode() == 0600));

      lmdb::env_t dstenv{ std::move(srcenv) };
      REQUIRE(dstenv.lasterror());
      REQUIRE((dstenv.maxdbs(100) && dstenv.maxdbs() == 100));
      REQUIRE((dstenv.maxreaders(101) && dstenv.maxreaders() == 101));
      REQUIRE((dstenv.mmapsize(1024 * 1024) && dstenv.mmapsize() == 1024 * 1024));
      REQUIRE((dstenv.mode(0600) && dstenv.mode() == 0600));
   }

   SECTION("test env_t move operator")
   {
      lmdb::env_t srcenv;
      REQUIRE(srcenv.lasterror());
      REQUIRE((srcenv.maxdbs(100) && srcenv.maxdbs() == 100));
      REQUIRE((srcenv.maxreaders(101) && srcenv.maxreaders() == 101));
      REQUIRE((srcenv.mmapsize(1024 * 1024) && srcenv.mmapsize() == 1024 * 1024));
      REQUIRE((srcenv.mode(0600) && srcenv.mode() == 0600));

      lmdb::env_t dstenv;
      REQUIRE(dstenv.lasterror());
      REQUIRE((dstenv.maxdbs(100) && dstenv.maxdbs() == 100));
      REQUIRE((dstenv.maxreaders(101) && dstenv.maxreaders() == 101));
      REQUIRE((dstenv.mmapsize(1024 * 1024) && dstenv.mmapsize() == 1024 * 1024));
      REQUIRE((dstenv.mode(0600) && dstenv.mode() == 0600));

      dstenv = std::move(srcenv);
      REQUIRE(dstenv.lasterror());
      REQUIRE((dstenv.maxdbs(100) && dstenv.maxdbs() == 100));
      REQUIRE((dstenv.maxreaders(101) && dstenv.maxreaders() == 101));
      REQUIRE((dstenv.mmapsize(1024 * 1024) && dstenv.mmapsize() == 1024 * 1024));
      REQUIRE((dstenv.mode(0600) && dstenv.mode() == 0600));
   }

   SECTION("test env_t open() default flags")
   {
      namespace fs = std::filesystem;
      lmdb::env_t env(MDB_EPHEMERAL);
      auto rc = env.open();
      REQUIRE(rc);
      if (rc)
      {
         REQUIRE(env.handle() != nullptr);
         REQUIRE(env.isopen());
         REQUIRE(env.getflags() == MDB_EPHEMERAL);
         REQUIRE(env.maxdbs() == lmdb::DEFAULT_MAXDBS);
         REQUIRE(env.maxreaders() == lmdb::DEFAULT_MAXREADERS);
         REQUIRE(env.mmapsize() == lmdb::DEFAULT_MMAPSIZE);
         REQUIRE(env.mode() == lmdb::DEFAULT_MODE);
         REQUIRE(env.maxkeysize() == 511);
         REQUIRE(env.path() == std::filesystem::current_path());
         REQUIRE(env.check() == 0);
         REQUIRE(env.flush() == 0);
         env.close();
         REQUIRE(!env.exist());

      }
      else
      {
         REQUIRE(!env.isopen());
         REQUIRE(env.handle() == nullptr);
      }
   }

   SECTION("test env_t open() with flags MDB_RDONLY and MDB_NOLOCK")
   {
      namespace fs = std::filesystem;
      lmdb::env_t env;
      // create the environment with default arguments
      auto rc = env.open();
      REQUIRE(rc);
      if (rc)
      {
         REQUIRE(env.handle() != nullptr);
         REQUIRE(env.isopen());
         REQUIRE(env.exist());
         REQUIRE(rc);
         env.close();
      }
      // now the environment files have been created, reopen them with MDB_RDONLY
      REQUIRE(env.setflags(MDB_RDONLY, MDB_NOLOCK, MDB_EPHEMERAL));
      rc = env.open();
      if (rc)
      {
         REQUIRE(env.handle() != nullptr);
         REQUIRE(env.isopen());
         REQUIRE(env.exist());
         REQUIRE(env.getflags() == (MDB_RDONLY | MDB_NOLOCK | MDB_EPHEMERAL));
         REQUIRE(env.maxdbs() == lmdb::DEFAULT_MAXDBS);
         REQUIRE(env.maxreaders() == lmdb::DEFAULT_MAXREADERS);
         REQUIRE(env.mmapsize() == lmdb::DEFAULT_MMAPSIZE);
         REQUIRE(env.mode() == lmdb::DEFAULT_MODE);
         REQUIRE(env.maxkeysize() == 511);
         REQUIRE(env.path() == std::filesystem::current_path());
         REQUIRE(env.check() == 0);
         env.close();
         REQUIRE(!env.exist());
      }
      else
      {
         REQUIRE(!env.isopen());
         REQUIRE(env.handle() == nullptr);
      }
   }

   SECTION("test env_t open() with flags MDB_NOSUBDIR")
   {
      namespace fs = std::filesystem;
      lmdb::env_t env;
      REQUIRE(env.setflags(MDB_NOSUBDIR, MDB_EPHEMERAL));
      auto rc = env.open();
      REQUIRE(rc);
      if (rc)
      {
         REQUIRE(env.handle() != nullptr);
         REQUIRE(env.isopen());
         REQUIRE(env.exist());
         REQUIRE(env.getflags() == (MDB_NOSUBDIR | MDB_EPHEMERAL));
         REQUIRE(env.maxdbs() == lmdb::DEFAULT_MAXDBS);
         REQUIRE(env.maxreaders() == lmdb::DEFAULT_MAXREADERS);
         REQUIRE(env.mmapsize() == lmdb::DEFAULT_MMAPSIZE);
         REQUIRE(env.mode() == lmdb::DEFAULT_MODE);
         REQUIRE(env.maxkeysize() == 511);
         REQUIRE((env.path() == std::filesystem::current_path() / "lmdb.mdb"));
         REQUIRE(env.check() == 0);
         env.close();
         REQUIRE(!env.exist());
      }
      else
      {
         REQUIRE(!env.isopen());
         REQUIRE(env.handle() == nullptr);
      }
   }
}


TEST_CASE("txn_t lifecycle and basic operations", "[txn_t]") 
{
   lmdb::env_t env(MDB_EPHEMERAL);
   REQUIRE(env.open().ok());

   SECTION("Begin and commit read-write transaction") 
   {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE(txn.begin().ok());
      REQUIRE(txn.pending());
      REQUIRE(txn.commit().ok());
      REQUIRE_FALSE(txn.pending());
   }

   SECTION("Begin and abort read-write transaction") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE(txn.begin().ok());
      REQUIRE(txn.pending());
      REQUIRE(txn.abort().ok());
      REQUIRE_FALSE(txn.pending());
   }

   SECTION("Reset and renew read-only transaction") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readonly);
      REQUIRE(txn.begin().ok());
      REQUIRE(txn.pending());

      REQUIRE(txn.reset().ok());
      // No handle use allowed here

      REQUIRE(txn.renew().ok());
      REQUIRE(txn.pending());

      REQUIRE(txn.abort().ok());
   }

   SECTION("Double begin fails") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readonly);
      REQUIRE(txn.begin().ok());
      REQUIRE_FALSE(txn.begin().ok()); // Already active
      REQUIRE(txn.abort().ok());
   }

   SECTION("Abort without begin fails") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE_FALSE(txn.abort().ok()); // No txn active
   }

   SECTION("Commit without begin fails") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE_FALSE(txn.commit().ok());
   }

   SECTION("Reset fails on write transaction") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE(txn.begin().ok());
      REQUIRE_FALSE(txn.reset().ok());
      REQUIRE(txn.abort().ok());
   }

   SECTION("Renew fails on write transaction") {
      lmdb::txn_t txn(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE(txn.begin().ok());
      REQUIRE_FALSE(txn.renew().ok());
      REQUIRE(txn.abort().ok());
   }

   SECTION("Move construction") {
      lmdb::txn_t original(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE(original.begin().ok());

      lmdb::txn_t moved = std::move(original);
      REQUIRE(moved.pending());
      REQUIRE_FALSE(original.pending());
      REQUIRE(moved.commit().ok());
   }

   SECTION("Move assignment") {
      lmdb::txn_t txn1(env, lmdb::txn_t::type_t::readwrite);
      REQUIRE(txn1.begin().ok());

      lmdb::txn_t txn2(env, lmdb::txn_t::type_t::readwrite);
      txn2 = std::move(txn1);

      REQUIRE(txn2.pending());
      REQUIRE_FALSE(txn1.pending());
      REQUIRE(txn2.commit().ok());
   }
   env.close();
}

