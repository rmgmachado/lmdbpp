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
#include "lmdbpp.h"

using namespace lmdb;

TEST_CASE("lmdbpp.h environment_t class tests", "[environment_t]")
{
   SECTION("Test enviroment_t default constructor and destructor")
   {
      environment_t env;
      REQUIRE(env.handle() == nullptr);
      REQUIRE(env.path().size() == 0);
      REQUIRE(env.path() == "");
      REQUIRE(env.max_readers() == 0);
      REQUIRE(env.max_keysize() == 0);
      REQUIRE(env.max_tables() == 0);
   }
   SECTION("test environment_t move constructor")
   {
      // before the move
      std::string path(".\\");
      environment_t env;
      REQUIRE(env.startup(path).ok());
      REQUIRE(env.handle() != nullptr);
      REQUIRE(env.path().size() == path.size());
      REQUIRE(env.path() == path);
      REQUIRE(env.max_tables() == DEFAULT_MAXTABLES);

      environment_t env1{ std::move(env) };
      REQUIRE(env1.handle() != nullptr);
      REQUIRE(env1.path().size() == path.size());
      REQUIRE(env1.path() == path);
      REQUIRE(env1.max_tables() == DEFAULT_MAXTABLES);

      REQUIRE(env.handle() == nullptr);
      REQUIRE(env.path().size() == 0);
      REQUIRE(env.path() == "");
      REQUIRE(env.max_tables() == 0);
   }
   SECTION("test environment_t move operator")
   {
      // before the move
      std::string path(".\\");
      environment_t env;
      REQUIRE(env.startup(path).ok());
      REQUIRE(env.handle() != nullptr);
      REQUIRE(env.path().size() == path.size());
      REQUIRE(env.path() == path);
      REQUIRE(env.max_tables() == DEFAULT_MAXTABLES);

      environment_t env1;
      env1 = std::move(env);
      REQUIRE(env1.handle() != nullptr);
      REQUIRE(env1.path().size() == path.size());
      REQUIRE(env1.path() == path);
      REQUIRE(env1.max_tables() == DEFAULT_MAXTABLES);

      REQUIRE(env.handle() == nullptr);
      REQUIRE(env.path().size() == 0);
      REQUIRE(env.path() == "");
      REQUIRE(env.max_tables() == 0);
   }
   SECTION("test environment_t flush() method")
   {
      std::string path(".\\");
      environment_t env;
      REQUIRE(env.startup(path).ok());
      REQUIRE(env.handle() != nullptr);
      REQUIRE(env.flush().ok());
   }
}

TEST_CASE("lmdbpp.h transaction_t class tests", "[transaction_t]")
{
   std::string path(".\\");
   environment_t env;
   REQUIRE(env.startup(path).ok());
   REQUIRE(env.handle() != nullptr);

   SECTION("Test transaction_t constructor")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
   }
   SECTION("Test transaction_t begin() method with transaction_type_t::read_write")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(txn.handle() != nullptr);
   }
   SECTION("Test transaction_t begin() method with transaction_type_t::read_only")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
      REQUIRE(txn.begin(transaction_type_t::read_only).ok());
      REQUIRE(txn.handle() != nullptr);
   }
   SECTION("Test transaction_t commit() method with transaction_type_t::read_write")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(txn.handle() != nullptr);
      REQUIRE(txn.commit().ok());
      REQUIRE(txn.handle() == nullptr);
   }
   SECTION("Test transaction_t commit() method with transaction_type_t::read_only")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
      REQUIRE(txn.begin(transaction_type_t::read_only).ok());
      REQUIRE(txn.handle() != nullptr);
      REQUIRE(txn.commit().ok());
      REQUIRE(txn.handle() == nullptr);
   }
   SECTION("Test transaction_t abort() method with transaction_type_t::read_write")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(txn.handle() != nullptr);
      REQUIRE(txn.abort().ok());
      REQUIRE(txn.handle() == nullptr);
   }
   SECTION("Test transaction_t abort() method with transaction_type_t::read_only")
   {
      transaction_t txn(env);
      REQUIRE(txn.handle() == nullptr);
      REQUIRE(txn.begin(transaction_type_t::read_only).ok());
      REQUIRE(txn.handle() != nullptr);
      REQUIRE(txn.abort().ok());
      REQUIRE(txn.handle() == nullptr);
   }
}

template <typename T>
bool equal(const T& lhs, MDB_val* rhs)
{
   if (!rhs) return false;
   if (lhs.size() != rhs->mv_size) return false;
   return memcmp(&lhs[0], rhs->mv_data, rhs->mv_size) == 0;
}

TEST_CASE("lmdbpp.h data_t class tests", "[data_t]")
{
   SECTION("Test data_t default constructor")
   {
      data_t<std::string> dt;
      REQUIRE(dt.size() == 0);
      REQUIRE(dt.data()->mv_data == nullptr);
   }
   SECTION("Test data_t constructor")
   {
      std::string str{ "This is a string" };
      data_t dt(str);
      REQUIRE(dt.size() == str.size());
      REQUIRE(equal(str, dt.data()));
   }
   SECTION("Test data_t get() method")
   {
      std::string str{ "This is a string" };
      data_t dt(str);
      REQUIRE(dt.size() == str.size());
      REQUIRE(equal(str, dt.data()));
      std::string str1;
      dt.get(str1);
      REQUIRE(str == str1);
   }
   SECTION("Test data_t get() method when data_t size is 0")
   {
      std::string str{ "This is a string" };
      data_t<std::string> dt;
      REQUIRE(dt.size() ==0);
      REQUIRE(dt.data()->mv_data == nullptr);
      dt.get(str);
      REQUIRE(str == "");
      REQUIRE(str.size() == 0);
   }
   SECTION("Test data_t set() method")
   {
      std::string str{ "This is a string" };
      data_t<std::string> dt;
      REQUIRE(dt.size() == 0);
      REQUIRE(dt.data()->mv_data == nullptr);
      dt.set(str);
      REQUIRE(dt.size() == str.size());
      REQUIRE(equal(str, dt.data()));
   }
   SECTION("Test data_t set() method when string is empty")
   {
      std::string str;
      data_t<std::string> dt;
      REQUIRE(dt.size() == 0);
      REQUIRE(dt.data()->mv_data == nullptr);
      dt.set(str);
      REQUIRE(dt.size() == str.size());
      REQUIRE(dt.data()->mv_data == nullptr);
   }
}

TEST_CASE("lmdbpp.h table_t class tests", "[table_t]")
{
   std::string path(".\\");
   environment_t env;
   REQUIRE(env.startup(path).ok());
   REQUIRE(env.handle() != nullptr);
   transaction_t txn(env);

   SECTION("Test table_ constructor")
   {
      table_t tb(env);
      REQUIRE(tb.handle() == 0);
   }
   SECTION("Test table_t create() method")
   {
      std::string path{ "test.dbm" };
      table_t tb(env);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(tb.create(txn, path).ok());
   }
   SECTION("Test table_t put() and get() methods")
   {
      std::vector<std::pair<std::string, std::string>> data =
      {
           { "first", "first record" }
         , { "second", "second record" }
         , { "third", "third record" } 
      };
      std::string path{ "test.dbm" };
      table_t tb(env);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(tb.create(txn, path).ok());
      REQUIRE(txn.commit().ok());
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      for (auto [key, value] : data)
      {
         REQUIRE(tb.put(key, value).ok());
         std::string k{key}, v;
         REQUIRE(tb.get(k, k, v).ok());
         REQUIRE(k == key);
         REQUIRE(v == value);
      }
      REQUIRE(tb.drop().ok());
      REQUIRE(txn.commit().ok());
   }
   SECTION("Test table_t put() and get() methods with keyvalue_t")
   {
      std::vector<std::pair<std::string, std::string>> data =
      {
           { "first", "first record" }
         , { "second", "second record" }
         , { "third", "third record" }
      };
      std::string path{ "test.dbm" };
      table_t tb(env);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(tb.create(txn, path).ok());
      for (auto item : data)
      {
         REQUIRE(tb.put(item).ok());
         keyvalue_t kv;
         REQUIRE(tb.get(item.first, kv).ok());
         REQUIRE(kv.first == item.first);
         REQUIRE(kv.second == item.second);
      }
      REQUIRE(tb.drop().ok());
      REQUIRE(txn.commit().ok());
   }
   SECTION("Test table_t del()")
   {
      std::vector<std::pair<std::string, std::string>> data =
      {
           { "first", "first record" }
         , { "second", "second record" }
         , { "third", "third record" }
      };
      std::string path{ "test.dbm" };
      table_t tb(env);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(tb.create(txn, path).ok());
      for (auto [key, value] : data)
      {
         REQUIRE(tb.put(key, value).ok());
         std::string k{ key }, v;
         REQUIRE(tb.get(k, k, v).ok());
         REQUIRE(k == key);
         REQUIRE(v == value);
      }
      REQUIRE(tb.del(data[1].first, data[1].second).ok());
      REQUIRE(tb.get(data[1].first, data[1].first, data[1].second).nok());
      REQUIRE(tb.drop().ok());
      REQUIRE(txn.commit().ok());
   }
   SECTION("Test table_t del() with keyvalue_t")
   {
      std::vector<std::pair<std::string, std::string>> data =
      {
           { "first", "first record" }
         , { "second", "second record" }
         , { "third", "third record" }
      };
      std::string path{ "test.dbm" };
      table_t tb(env);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(tb.create(txn, path).ok());
      keyvalue_t kv;
      for (auto item : data)
      {
         REQUIRE(tb.put(item).ok());
         REQUIRE(tb.get(item.first, kv).ok());
         REQUIRE(kv.first == item.first);
         REQUIRE(kv.second == item.second);
      }
      REQUIRE(tb.del(data[1]).ok());
      REQUIRE(tb.get(data[1].first, data[1]).nok());
      REQUIRE(tb.drop().ok());
      REQUIRE(txn.commit().ok());
   }
   SECTION("Test table_t entries() method")
   {
      std::vector<std::pair<std::string, std::string>> data =
      {
           { "first", "first record" }
         , { "second", "second record" }
         , { "third", "third record" }
      };
      std::string path{ "test.dbm" };
      table_t tb(env);
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      REQUIRE(tb.create(txn, path).ok());
      keyvalue_t kv;
      for (auto item : data)
      {
         REQUIRE(tb.put(item).ok());
         REQUIRE(tb.get(item.first, kv).ok());
         REQUIRE(kv.first == item.first);
         REQUIRE(kv.second == item.second);
      }
      REQUIRE(tb.entries(txn) == data.size());
      REQUIRE(tb.drop().ok());
      REQUIRE(txn.commit().ok());
   }
}

using dataset_t = std::vector<std::pair<std::string, std::string>>;

status_t populate(table_t& tbl, const dataset_t& ds) noexcept
{
   status_t status;
   for (auto item : ds)
   {
      if (status = tbl.put(item); status.nok()) return status;
   }
   return status;
}

TEST_CASE("lmdbpp.h cursor_t class tests", "[cursor_t]")
{
   dataset_t data =
   {
        { "first", "first record" }
      , { "second", "second record" }
      , { "third", "third record" }
   };

   std::string path(".\\");
   environment_t env;
   REQUIRE(env.startup(path).ok());
   REQUIRE(env.handle() != nullptr);
   transaction_t txn(env);
   REQUIRE(txn.begin(transaction_type_t::read_write).ok());
   table_t tb(env);
   REQUIRE(tb.create(txn, path).ok());
   REQUIRE(populate(tb, data).ok());
   REQUIRE(txn.commit().ok());
   REQUIRE(txn.begin(transaction_type_t::read_only).ok());

   SECTION("Test cursor_t class first() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.first(key, value).ok());
      REQUIRE((key == data[0].first && value == data[0].second));
   }
   SECTION("Test cursor_t class first() method with keyvalue_t")
   {
      cursor_t cursor(txn, tb);
      keyvalue_t kv;
      REQUIRE(cursor.first(kv).ok());
      REQUIRE(kv == data[0]);
   }
   SECTION("Test cursor_t class first() method key only")
   {
      cursor_t cursor(txn, tb);
      std::string key;
      REQUIRE(cursor.first(key).ok());
      REQUIRE(key == data[0].first);
   }
   SECTION("Test cursor_t class last() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.last(key, value).ok());
      REQUIRE((key == data[2].first && value == data[2].second));
   }
   SECTION("Test cursor_t class last() method with keyvalue_t")
   {
      cursor_t cursor(txn, tb);
      keyvalue_t kv;
      REQUIRE(cursor.last(kv).ok());
      REQUIRE(kv == data[2]);
   }
   SECTION("Test cursor_t class last() method key only")
   {
      cursor_t cursor(txn, tb);
      std::string key;
      REQUIRE(cursor.last(key).ok());
      REQUIRE(key == data[2].first);
   }
   SECTION("Test cursor_t class next() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.first(key, value).ok());
      REQUIRE((key == data[0].first && value == data[0].second));
      REQUIRE(cursor.next(key, value).ok());
      REQUIRE((key == data[1].first && value == data[1].second));
      REQUIRE(cursor.last(key, value).ok());
      REQUIRE((key == data[2].first && value == data[2].second));
      REQUIRE(cursor.next(key, value).nok());
   }
   SECTION("Test cursor_t class next() method keyvalue_t")
   {
      cursor_t cursor(txn, tb);
      keyvalue_t kv;
      REQUIRE(cursor.first().ok());
      REQUIRE(cursor.next(kv).ok());
      REQUIRE(kv == data[1]);
      REQUIRE(cursor.last().ok());
      REQUIRE(cursor.next().nok());
   }
   SECTION("Test cursor_t class next() method key only")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.first(key, value).ok());
      REQUIRE((key == data[0].first && value == data[0].second));
      REQUIRE(cursor.next(key).ok());
      REQUIRE(key == data[1].first);
      REQUIRE(cursor.last().ok());
      REQUIRE(cursor.next().nok());
   }
   SECTION("Test cursor_t class prior() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.last(key, value).ok());
      REQUIRE((key == data[2].first && value == data[2].second));
      REQUIRE(cursor.prior(key, value).ok());
      REQUIRE((key == data[1].first && value == data[1].second));
      REQUIRE(cursor.first(key, value).ok());
      REQUIRE((key == data[0].first && value == data[0].second));
      REQUIRE(cursor.prior(key, value).nok());
   }
   SECTION("Test cursor_t class prior() method with keyvalue_t")
   {
      cursor_t cursor(txn, tb);
      keyvalue_t kv;
      REQUIRE(cursor.last(kv).ok());
      REQUIRE(kv == data[2]);
      REQUIRE(cursor.prior(kv).ok());
      REQUIRE(kv == data[1]);
      REQUIRE(cursor.first().ok());
      REQUIRE(cursor.prior().nok());
   }
   SECTION("Test cursor_t class prior() method key only")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.last(key, value).ok());
      REQUIRE((key == data[2].first && value == data[2].second));
      REQUIRE(cursor.prior(key).ok());
      REQUIRE(key == data[1].first);
   }
   SECTION("Test cursor_t class find() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.find(data[1].first, key, value).ok());
      REQUIRE((key == data[1].first && value == data[1].second));
   }
   SECTION("Test cursor_t class search() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.search("m", key, value).ok());
      REQUIRE((key == data[1].first && value == data[1].second));
   }
   SECTION("Test cursor_t class seek() method")
   {
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.seek(data[1].first).ok());
      REQUIRE(cursor.current(key, value).ok());
      REQUIRE((key == data[1].first && value == data[1].second));
   }
   SECTION("Test cursor_t class put() method")
   {
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      cursor_t cursor(txn, tb);
      std::pair<std::string, std::string> val = { "forth", "fourth record" };
      std::string key{ val.first }, value{ val.second };
      REQUIRE(cursor.put(key, value).ok());
      REQUIRE((key == val.first && value == val.second));
      REQUIRE(cursor.find(val.first, key, value).ok());
      REQUIRE((key == val.first && value == val.second));
   }
   SECTION("Test cursor_t class del() method")
   {
      REQUIRE(txn.begin(transaction_type_t::read_write).ok());
      cursor_t cursor(txn, tb);
      std::string key, value;
      REQUIRE(cursor.first(key, value).ok());
      REQUIRE((key == data[0].first && value == data[0].second));
      REQUIRE(cursor.del().ok());
      REQUIRE(cursor.first(key, value).ok());
      REQUIRE((key == data[1].first && value == data[1].second));
   }
}

