#include "gtest/gtest.h"
#include "sqlite3cpp.h"
#include <iostream>

TEST(basic, construct) {
    using namespace sqlite3cpp;
    database d("test.db");
}

TEST(basic, query) {
    using namespace sqlite3cpp;
    database db("test.db");
    auto c = db.make_cursor();

    c.executescript(
      "begin;"
      "create table T (a INT, b TEXT);"
      "insert into T values(1, 'test1');"
      "insert into T values(2, 'test2');"
      "insert into T values(2, 'abc');"
      "insert into T values(3, 'test3');"
      "commit;"
      );

    char const *query = "select * from T where a > ? and a < ? and b like ?";
    std::string pattern = "test%";

    int idx = 0;
    for(auto const &row : c.execute(query, 1, 3, pattern)) {
        int a; std::string b;
        std::tie(a, b) = row.get<int, std::string>();

        ASSERT_EQ(2, a);
        ASSERT_STREQ("test2", b.c_str());
        std::cout << idx++ << ": " << a << "," << b << "\n";
    }
    ::remove("test.db");
}


TEST(basic, wrap_function) {
    std::function<int(int)> c;
    auto f = sqlite3cpp::sqlval2cpp::make_invoker(c);
}


TEST(basic, create_scalar) {
    using namespace sqlite3cpp;
    database db("test.db");
    auto c = db.make_cursor();

    c.executescript(
      "begin;"
      "create table T (a INT, b TEXT);"
      "insert into T values(1, 'test1');"
      "insert into T values(2, 'test2');"
      "insert into T values(2, 'abc');"
      "insert into T values(3, 'test3');"
      "commit;"
      );

    int x = 123;

    db.create_scalar("plus123", [&x](int input) {
        return x + input;
    });

    char const *query = "select plus123(a) from T;";

    int idx = 0;
    int expected[] = { 123+1, 123+2, 123+2, 123+3 };
    for(auto const &row : c.execute(query)) {
        int a = std::get<0>(row.get<int>());
        ASSERT_EQ(expected[idx], a);
        std::cout << idx++ << ": " << a << "\n";
    }
    ::remove("test.db");
}
