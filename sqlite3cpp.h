/*****************************************************************************
 * The BSD 3-Clause License
 *
 * Copyright (c) 2019, Acer Yun-Tse Yang All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "sqlite3cpp_export.h"
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
extern "C" {
#include "sqlite3.h"
}

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

#define C_STYLE_DELETER(T, F)                 \
  struct T##_deleter {                        \
    void operator()(T *mem) const { F(mem); } \
  }

namespace sqlite3cpp {

/**
 * Forwarded decls
 */
struct error;
struct database;
struct cursor;
struct row_iter;
struct row;
struct aggregate;

C_STYLE_DELETER(sqlite3, sqlite3_close);
C_STYLE_DELETER(sqlite3_stmt, sqlite3_finalize);

struct error : std::exception {
  error(int code) noexcept : code(code) {}
  char const *what() const noexcept { return sqlite3_errstr(code); }
  int code;
};

struct SQLITE3CPP_EXPORT row {
  // Retrieve tuple of row values from this row e.g.
  //
  // database db(":memory:");
  // cursor csr = db.make_cursor();
  // char const* query = "select IntColumn, StrColumn from MyTable";
  //
  // for(auto const& row : c.execute(query)) {
  //   auto [x,y] = row.to<int, std::string>();
  // }
  //
  // Values are **converted** to types specified in template parameters. The
  // conversion is built-in in sqlite3 as described in
  // https://www.sqlite.org/c3ref/column_blob.html
  //
  // The sqlite3cpp provides **references** to row values by
  //
  // auto [s] = row.to<std::string_view>();
  //
  // Be caution that those references are expired after advancing row_iter.
  //
  // Retrieving value as string demands allocations in sqlite3. Even
  // std::string_view could introduce allocation if conversion was needed. When
  // OOM occurs, sqlite3cpp will raise exceptions of type sqlite3cpp::error.
  // If exceptions are not preferred, one can use std::optional<std::string> or
  // std::optional<std::stirng_view> as type parameters such that no exceptions
  // shall be raised as of OOM.
  template <typename... Cols>
  std::tuple<Cols...> to() const;

  // Get underlying sqlite3_stmt pointer.
  sqlite3_stmt *get() const noexcept { return m_stmt; }

 private:
  friend struct row_iter;
  row() = default;
  sqlite3_stmt *m_stmt = nullptr;
  sqlite3 *m_db = nullptr;
};

struct SQLITE3CPP_EXPORT row_iter {
  row_iter &operator++();
  bool operator==(row_iter const &i) const noexcept;
  bool operator!=(row_iter const &i) const noexcept;
  row const &operator*() const noexcept { return m_row; }
  row const *operator->() const noexcept { return &m_row; }

 private:
  friend struct cursor;
  row_iter() noexcept {}
  row_iter(cursor &csr) noexcept;
  cursor *m_csr = nullptr;
  row m_row;
};

struct SQLITE3CPP_EXPORT cursor {
  cursor(cursor &&) = default;
  // Execute a single SQL statement with binded arguments.
  //
  // database db(":memory:");
  // cursor csr = db.make_cursor();
  // csr.execute("insert into MyTable values(?)", 123);
  //
  // Binded text types, i.e. string, string_view, char const*, are referenced by
  // row_iter and cursor. They need to be valid until next call to |execute()|,
  // cursor reaches end of life.
  //
  // Only poisitioned binding is supported currently. Named
  // binding is not supported yet.
  template <typename... Args>
  cursor &execute(std::string const &sql, Args &&... args);

  // Execute multiple SQL statements.
  cursor &executescript(std::string const &sql);

  // Row iterator to begin of query results. This row_iter becomes invalid after
  // the cursor it referenced being detroyed.
  row_iter begin() noexcept { return row_iter(*this); }

  // Row itertor to end of query results (next to the last one of result).
  row_iter end() noexcept { return row_iter(); }

  // Get underlying sqlite3_stmt pointer.
  sqlite3_stmt *get() const noexcept { return m_stmt.get(); }

 private:
  void step();
  friend struct row_iter;
  friend struct database;
  cursor(database const &db) noexcept;
  sqlite3 *m_db;
  std::unique_ptr<sqlite3_stmt, sqlite3_stmt_deleter> m_stmt;
};

struct SQLITE3CPP_EXPORT database {
  using xfunc_t = std::function<void(sqlite3_context *, int, sqlite3_value **)>;
  using xfinal_t = std::function<void(sqlite3_context *)>;
  using xreset_t = std::function<void()>;

  // Create a database connection to |urn|. |urn| could be `:memory:` or a filename.
  // |urn| should be encoded in UTF-8.
  database(std::string const &urn);

  // Create a cursor per current database for executing SQL statements.
  cursor make_cursor() const noexcept;

  // Get underlying sqlite3 (database) pointer.
  sqlite3 *get() const noexcept { return m_db.get(); }

  // Shortcut for calling |cursor::execute|.
  template <typename... Args>
  cursor execute(std::string const &sql, Args &&... args);

  // Shortcut for calling |cursor::executescript|.
  cursor executescript(std::string const &sql);

  // Create a scalar function in current database. |func| can be lambda or other
  // std::function<> compatible types. e.g.
  //
  // database db(":memory:");
  //
  // db.create_scalar("myScalar", [](int x, int y) { return x + y; });
  // for (auto const &row : db.execute("select myScalar(1, 1)")) {
  //   auto [val] = row.to<int>();
  //   EXPECT_EQ(2, val);
  // }
  //
  // Arity and types of function parameters are deduced automatically. Supported
  // parameter types are int, int64_t, double, std::string, and
  // std::string_view.
  template <typename FUNC>
  void create_scalar(std::string const &name, FUNC func,
                     int flags = SQLITE_UTF8 | SQLITE_DETERMINISTIC);

  // Create an aggregate in current database.
  // An aggregate is specified in the |AG| type parameter. It must provide
  // interfaces as follows:
  //
  // AG::AG()
  // void AG::step(T val)
  // R AG::finalize()
  //
  // where T can be int, int64_t, double, std::string, or std::string_view
  // and R can be int, int64_t, double, std::string.
  template <typename AG>
  void create_aggregate(std::string const &name,
                        int flags = SQLITE_UTF8 | SQLITE_DETERMINISTIC);

  // Get version string of sqlite3cpp (not version of sqlite3).
  std::string version() const;

 private:
  struct aggregate_wrapper_t {
    xfunc_t step;
    xfinal_t fin;
    xreset_t reset;
    xreset_t release;
  };

  static void forward(sqlite3_context *ctx, int argc, sqlite3_value **argv);
  static void dispose(void *user_data);
  static void step_ag(sqlite3_context *ctx, int argc, sqlite3_value **argv);
  static void final_ag(sqlite3_context *ctx);
  static void dispose_ag(void *user_data);

  std::unique_ptr<sqlite3, sqlite3_deleter> m_db;
};

}  // namespace sqlite3cpp
#ifdef _WIN32
#pragma warning(pop)
#endif

#include "sqlite3cpp.ipp"
