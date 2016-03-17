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

TEST_F(DBTest, query) {
    using namespace sqlite3cpp;
    auto c = basic_dataset().make_cursor();
    char const *query = "select * from T where a > ? and a < ? and b like ?";
    std::string pattern = "test%";

    int idx = 0;
    for(auto const &row : c.execute(query, 1, 3, pattern)) {
        int a; std::string b;
        std::tie(a, b) = row.get<int, std::string>();

        EXPECT_EQ(2, a);
        EXPECT_STREQ("test2", b.c_str());
        std::cout << idx++ << ": " << a << "," << b << "\n";
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

    /* TODO Add functor test
    struct plus_x {
        plus_x(int x) : m_x(x) {}
        int operator()(int input) const {
            return input + m_x;
        }
        int m_x;
    };
    db.create_scalar("plus123", plus_x(123));

    */
    int x = 123;
    basic_dataset().create_scalar("plus123", [x](int input) {
        return x + input;
    });

    basic_dataset().create_scalar("mutiply", [](int x, int y) {
        return x * y;
    });

    char const *query = "select plus123(a), mutiply(a,a) from T;";

    int idx = 0;
    int expected[4][2] = {
        {123+1, 1*1},
        {123+2, 2*2},
        {123+2, 2*2},
        {123+3, 3*3}
    };
    for(auto const &row : c.execute(query)) {
        int a, b;
        std::tie(a, b) = row.get<int, int>();
        EXPECT_EQ(expected[idx][0], a);
        EXPECT_EQ(expected[idx][1], b);
        std::cout << idx++ << ": " << a << ", " << b << "\n";
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
        std::tie(a) = row.get<double>();
        EXPECT_DOUBLE_EQ(0.81649658092772603, a);
    }
}

