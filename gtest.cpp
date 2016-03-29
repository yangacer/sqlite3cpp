/*****************************************************************************
 * Copyright (c) 2016, Acer Yun-Tse Yang All rights reserved.
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
#include "gtest/gtest.h"
#include "sqlite3cpp.h"
#include <iostream>

struct DBTest : ::testing::Test {
    DBTest()
    {}

    virtual ~DBTest() {}

    virtual void SetUp() {
        m_basic_dataset.reset(new sqlite3cpp::database(":memory:"));

        auto c = basic_dataset().make_cursor();
        c.executescript(
          "begin;"
          "create table T (a INT, b TEXT);"
          "insert into T values(1, 'test1');"
          "insert into T values(2, 'test2');"
          "insert into T values(2, 'abc');"
          "insert into T values(3, 'test3');"
          "commit;"
          );
    }

    virtual void TearDown() {
        m_basic_dataset.reset();
    }

    sqlite3cpp::database &basic_dataset() {
        return *m_basic_dataset;
    }

private:
    std::unique_ptr<sqlite3cpp::database> m_basic_dataset;
};

TEST(basic, construct) {
    using namespace sqlite3cpp;
    database d(":memory:");
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

    int idx = 0;
    for(auto const &row : c.execute(query, 1, 3, pattern)) {
        int a; std::string b;
        std::tie(a, b) = row.to<int, std::string>();

        EXPECT_EQ(2, a);
        EXPECT_STREQ("test2", b.c_str());
    }
}


TEST(basic, wrap_function) {
    using namespace sqlite3cpp;
    using namespace std;
    using namespace std::placeholders;

    function<int(int)> c;
    auto f = detail::make_invoker(move(c));

    struct functor {
        void step(int x){ }
        int finalize(){
            return 0;
        }
    };
    functor fr;
    detail::bind_this(&functor::step, &fr);
}


TEST_F(DBTest, create_scalar) {
    using namespace sqlite3cpp;
    auto c = basic_dataset().make_cursor();

    struct minus_x {
        minus_x(int x) : m_x(x) {}
        int operator()(int input) const {
            return input - m_x;
        }
        int m_x;
    };
    basic_dataset().create_scalar("minus123", minus_x(123));

    int x = 123;
    basic_dataset().create_scalar("plus123", [x](int input) {
        return x + input;
    });

    basic_dataset().create_scalar("mutiply", [](int x, int y) {
        return x * y;
    });

    char const *query = "select plus123(a), mutiply(a,a), minus123(a) from T;";

    int idx = 0;
    int expected[4][3] = {
        {123+1, 1*1, 1 - 123},
        {123+2, 2*2, 2 - 123},
        {123+2, 2*2, 2 - 123},
        {123+3, 3*3, 3 - 123}
    };
    for(auto const &row : c.execute(query)) {
        int a, b, c;
        std::tie(a, b, c) = row.to<int, int, int>();
        EXPECT_EQ(expected[idx][0], a);
        EXPECT_EQ(expected[idx][1], b);
        EXPECT_EQ(expected[idx][2], c);
        idx++;
    }
}

TEST_F(DBTest, create_aggregate) {

    using namespace sqlite3cpp;

    struct stdev {
        stdev() : m_cnt(0), m_sum(0), m_sq_sum(0) {}
        void step(int val) {
            m_cnt ++;
            m_sum += val;
            m_sq_sum += val * val;

        }
        double finalize() {
            auto avg = (double)m_sum / m_cnt;
            return std::sqrt((double)(m_sq_sum - avg * avg * m_cnt) / (m_cnt -1));
        }
        size_t m_cnt;
        int m_sum, m_sq_sum;
    };

    basic_dataset().create_aggregate<stdev>("stdev");
    auto c = basic_dataset().make_cursor();

    char const *query = "select stdev(a) from T";
    for(auto const &row : c.execute(query)) {
        double a;
        std::tie(a) = row.to<double>();
        // XXX Platfomr dependant
        EXPECT_DOUBLE_EQ(0.81649658092772603, a);
    }
}

TEST_F(DBTest, error_handle) {
    using namespace sqlite3cpp;

    auto c = basic_dataset().make_cursor();

    try {
        c.execute("invalid sql");
        FAIL() << "Expect throw";
    } catch(error const &e) {
        EXPECT_EQ(SQLITE_ERROR, e.code);
        EXPECT_STREQ("SQL logic error or missing database", e.what());
    } catch(...) {
        FAIL() << "sqlite3cpp::error should be caught";
    }

    try {
        c.executescript("invalid sql");
        FAIL() << "Expect throw";
    } catch(error const &e) {
        EXPECT_EQ(SQLITE_ERROR, e.code);
        EXPECT_STREQ("SQL logic error or missing database", e.what());
    } catch(...) {
        FAIL() << "sqlite3cpp::error should be caught";
    }

    try {
        c.execute("select * from T", 123); // invalid bind
    } catch(error const &e) {
        EXPECT_EQ(SQLITE_RANGE, e.code);
        EXPECT_STREQ("bind or column index out of range", e.what());
    } catch(...) {
        FAIL() << "sqlite3cpp::error should be caught";
    }

}

