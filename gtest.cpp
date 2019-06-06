/*****************************************************************************
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
#ifdef _WIN32
#pragma warning(disable : 4819)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "gtest/gtest.h"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include "sqlite3cpp.h"

[[maybe_unused]]
static void trace_print(void *ctx, char const *stmt) { printf("%s\n", stmt); }

struct DBTest : ::testing::Test {
  DBTest() {}

  virtual ~DBTest() {}

  virtual void SetUp() {
    m_basic_dataset.reset(new sqlite3cpp::database(":memory:"));

    // sqlite3_trace(m_basic_dataset->get(), &trace_print, nullptr);

    auto c = basic_dataset().make_cursor();
    c.executescript(
        "begin;"
        "create table T (a INTEGER, b TEXT);"
        "insert into T values(1, 'test1');"
        "insert into T values(2, 'test2');"
        "insert into T values(2, 'abc');"
        "insert into T values(3, 'test3');"
        "create table AllTypes (i INTEGER, r REAL, t TEXT);"
        "create table InsTest (a INTEGER, b TEXT);"
        "commit;");
  }

  virtual void TearDown() { m_basic_dataset.reset(); }

  sqlite3cpp::database &basic_dataset() { return *m_basic_dataset; }

 private:
  std::unique_ptr<sqlite3cpp::database> m_basic_dataset;
};

TEST(basic, construct) {
  using namespace sqlite3cpp;
  database d(":memory:");
}

TEST_F(DBTest, insert_many) {
  struct record {
    int i;
    std::string s;
  };

  std::vector<record> records = {
      {9, "test"}, {9, "test"}, {9, "test"}, {9, "test"},
  };

  auto c = basic_dataset().make_cursor();

  for (auto const &r : records) {
    c.execute("insert into InsTest values(?,?)", r.i, r.s);
  }

  for (auto const &row : c.execute("select * from InsTest")) {
    record rec;

    std::tie(rec.i, rec.s) = row.to<int, std::string>();
    EXPECT_EQ(9, rec.i);
    EXPECT_EQ("test", rec.s);
  }
}

TEST_F(DBTest, supported_types) {
  using namespace sqlite3cpp;

  auto c = basic_dataset().make_cursor();

  char const *c_str = "c string";
  std::string cpp_str = "cpp string";

  char const *str = "cpp ref string";
  std::string_view cpp_ref_str = str;

  c.execute("insert into AllTypes values(?,?,?)", 123, 123.123, c_str);

  c.execute("insert into AllTypes values(?,?,?)", nullptr, 123.123, cpp_str);

  c.execute("insert into AllTypes values(?,?,?)", 123, nullptr, cpp_ref_str);

  auto iter = c.execute("select * from AllTypes").begin();

  auto [i, d, s] = iter->to<int, double, std::string_view>();
  EXPECT_EQ(123, i);
  EXPECT_EQ(123.123, d);
  EXPECT_STREQ(c_str, s.data());
  ++iter;

  std::tie(i, d, s) = iter->to<int, double, std::string_view>();
  EXPECT_STREQ(cpp_str.c_str(), s.data());
  ++iter;

  std::tie(i, d, s) = iter->to<int, double, std::string_view>();
  EXPECT_STREQ(cpp_ref_str.data(), s.data());
}

TEST_F(DBTest, row_iter) {
  using namespace sqlite3cpp;

  auto c = basic_dataset().make_cursor();

  c.executescript("create table Empty (a);");
  c.execute("select * from Empty");

  EXPECT_EQ(c.begin(), c.end());

  c.execute("insert into Empty values(?)", 123);
  c.execute("select * from Empty");

  EXPECT_NE(c.begin(), c.end());
  EXPECT_EQ(++c.begin(), c.end());
  EXPECT_EQ(c.begin(), c.end());
}

TEST_F(DBTest, bind_null) {
  auto c = basic_dataset().make_cursor();

  c.execute("create table T2 (a);");
  c.execute("insert into T2 values(?)", nullptr);
  c.execute("select count(*) from T2 where a is NULL");

  int cnt = 0;
  std::tie(cnt) = c.begin()->to<int>();
  EXPECT_EQ(1, cnt);
}

TEST_F(DBTest, query) {
  using namespace sqlite3cpp;
  auto c = basic_dataset().make_cursor();
  char const *query = "select * from T where a > ? and a < ? and b like ?";
  std::string pattern = "test%";

  for (auto const &row : c.execute(query, 1, 3, "test%")) {
    int a;
    std::string b;
    std::tie(a, b) = row.to<int, std::string>();

    EXPECT_EQ(2, a);
    EXPECT_STREQ("test2", b.c_str());
  }
}

TEST_F(DBTest, query_with_string_view) {
  using namespace sqlite3cpp;

  auto c = basic_dataset().make_cursor();
  char const *query = "select * from T where a > ? and a < ? and b like ?";
  std::string pattern = "test%";

  for (auto const &row : c.execute(query, 1, 3, "test%")) {
    auto [a, b] = row.to<int, std::string_view>();

    EXPECT_EQ(2, a);
    EXPECT_STREQ("test2", b.data());
  }
}

TEST(basic, wrap_function) {
  using namespace sqlite3cpp;
  using namespace std;
  using namespace std::placeholders;

  function<int(int)> c;
  auto f = detail::make_invoker(move(c));

  struct functor {
    void step(int x) {}
    int finalize() { return 0; }
  };
  functor fr;
  detail::bind_this(&functor::step, &fr);
}

TEST_F(DBTest, create_scalar) {
  using namespace sqlite3cpp;
  auto c = basic_dataset().make_cursor();

  struct minus_x {
    minus_x(int x) : m_x(x) {}
    int operator()(int input) const { return input - m_x; }
    int m_x;
  };
  basic_dataset().create_scalar("minus123", minus_x(123));

  int x = 123;
  basic_dataset().create_scalar("plus123",
                                [x](int input) { return x + input; });

  basic_dataset().create_scalar("mutiply", [](int x, int y) { return x * y; });

  basic_dataset().create_scalar("strcat123",
                                [](std::string val) { return val + "_123"; });

  basic_dataset().create_scalar("divide",
                                [](int x, double y) { return (x + 9) / y; });

  char const *query =
      "select plus123(a), mutiply(a,a), minus123(a), strcat123(a),"
      "divide(a, a) from T;";

  int idx = 0;
  struct {
    int plus, mul, min;
    char const *cat;
    double div;
  } expected[4] = {{123 + 1, 1 * 1, 1 - 123, "1_123", (1 + 9) / 1.0},
                   {123 + 2, 2 * 2, 2 - 123, "2_123", (2 + 9) / 2.0},
                   {123 + 2, 2 * 2, 2 - 123, "2_123", (2 + 9) / 2.0},
                   {123 + 3, 3 * 3, 3 - 123, "3_123", (3 + 9) / 3.0}};

  for (auto const &row : c.execute(query)) {
    auto [a, b, c, d, e] = row.to<int, int, int, std::string, double>();
    EXPECT_EQ(expected[idx].plus, a);
    EXPECT_EQ(expected[idx].mul, b);
    EXPECT_EQ(expected[idx].min, c);
    EXPECT_EQ(expected[idx].cat, d);
    EXPECT_EQ(expected[idx].div, e);
    idx++;
  }
}

TEST_F(DBTest, max_int64) {
  auto c = basic_dataset().make_cursor();

  basic_dataset().create_scalar(
      "maxint64", []() { return std::numeric_limits<int64_t>::max(); });

  for (auto const &row : c.execute("select maxint64()")) {
    EXPECT_EQ(std::numeric_limits<int64_t>::max(),
              std::get<0>(row.to<int64_t>()));
  }
}

TEST_F(DBTest, create_aggregate) {
  using namespace sqlite3cpp;

  struct stdev {
    void step(int val) {
      m_cnt++;
      m_sum += val;
      m_sq_sum += val * val;
    }
    double finalize() {
      auto avg = (double)m_sum / m_cnt;
      return std::sqrt((double)(m_sq_sum - avg * avg * m_cnt) / (m_cnt - 1));
    }
    size_t m_cnt = 0;
    int m_sum = 0, m_sq_sum = 0;
  };

  struct commaMerge {
    void step(std::string_view val) {
      m_res.append(val);
      m_res.append(",");
    }
    std::string finalize() {
      if (!m_res.empty() && m_res.back() == ',') m_res.resize(m_res.size() - 1);
      return m_res;
    }
    std::string m_res;
  };

  basic_dataset().create_aggregate<stdev>("stdev");
  basic_dataset().create_aggregate<commaMerge>("commaMerge");

  auto c = basic_dataset().make_cursor();

  std::string query = "select stdev(a) from T";
  for (auto const &row : c.execute(query)) {
    auto [a] = row.to<double>();
    EXPECT_DOUBLE_EQ(0.81649658092772603, a);
  }

  query = "select commaMerge(b) from T";
  for (auto const &row : c.execute(query)) {
    auto [b] = row.to<std::string_view>();
    EXPECT_EQ("test1,test2,abc,test3", b);
  }

}

TEST_F(DBTest, error_handle) {
  using namespace sqlite3cpp;

  auto c = basic_dataset().make_cursor();

  try {
    c.execute("invalid sql");
    FAIL() << "Expect throw";
  } catch (error const &e) {
    EXPECT_EQ(SQLITE_ERROR, e.code);
    EXPECT_STREQ("SQL logic error", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }

  try {
    c.executescript("invalid sql");
    FAIL() << "Expect throw";
  } catch (error const &e) {
    EXPECT_EQ(SQLITE_ERROR, e.code);
    EXPECT_STREQ("SQL logic error", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }

  try {
    c.execute("select * from T", 123);  // invalid bind
  } catch (error const &e) {
    EXPECT_EQ(SQLITE_RANGE, e.code);
    EXPECT_STREQ("column index out of range", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }
}

TEST_F(DBTest, throw_in_custom_function) {
  auto c = basic_dataset().make_cursor();

  basic_dataset().create_scalar("bad_alloc", []() { throw std::bad_alloc(); });

  try {
    c.execute("select bad_alloc();");
  } catch (sqlite3cpp::error const &e) {
    EXPECT_EQ(SQLITE_NOMEM, e.code);
    EXPECT_STREQ("out of memory", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }

  basic_dataset().create_scalar("length_error",
                                []() { throw std::length_error("len err"); });

  try {
    c.execute("select length_error();");
  } catch (sqlite3cpp::error const &e) {
    EXPECT_EQ(SQLITE_ABORT, e.code);
    EXPECT_STREQ("query aborted", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }
}

TEST_F(DBTest, logic_error_in_aggregate) {
  auto c = basic_dataset().make_cursor();

  struct throw_in_step {
    void step(int a) { throw std::out_of_range("oops"); }
    int finalize() { return 0; }
  };

  basic_dataset().create_aggregate<throw_in_step>("throw_in_step");

  try {
    c.execute("select throw_in_step(a) from T");
  } catch (sqlite3cpp::error const &e) {
    EXPECT_EQ(SQLITE_ABORT, e.code);
    EXPECT_STREQ("query aborted", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }

  struct throw_in_final {
    void step(int a) {}
    int finalize() {
      throw std::out_of_range("oops");
      return 0;
    }
  };

  basic_dataset().create_aggregate<throw_in_final>("throw_in_final");

  try {
    c.execute("select throw_in_final(a) from T");
  } catch (sqlite3cpp::error const &e) {
    EXPECT_EQ(SQLITE_ABORT, e.code);
    EXPECT_STREQ("query aborted", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }
}

TEST_F(DBTest, bad_alloc_in_aggregate) {
  auto c = basic_dataset().make_cursor();

  struct throw_in_step {
    void step(int a) { throw std::bad_alloc(); }
    int finalize() { return 0; }
  };

  basic_dataset().create_aggregate<throw_in_step>("throw_in_step");

  try {
    c.execute("select throw_in_step(a) from T");
  } catch (sqlite3cpp::error const &e) {
    EXPECT_EQ(SQLITE_NOMEM, e.code);
    EXPECT_STREQ("out of memory", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }

  struct throw_in_final {
    void step(int a) {}
    int finalize() {
      throw std::bad_alloc();
      return 0;
    }
  };

  basic_dataset().create_aggregate<throw_in_final>("throw_in_final");

  try {
    c.execute("select throw_in_final(a) from T");
  } catch (sqlite3cpp::error const &e) {
    EXPECT_EQ(SQLITE_NOMEM, e.code);
    EXPECT_STREQ("out of memory", e.what());
  } catch (...) {
    FAIL() << "sqlite3cpp::error should be caught";
  }
}

#include "version.h"

TEST_F(DBTest, version) {
  EXPECT_STREQ(SQLITE3CPP_VERSION_STRING, basic_dataset().version().c_str());
}
