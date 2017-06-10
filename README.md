# sqlite3cpp

<a target="_blank" href="https://travis-ci.org/yangacer/sqlite3cpp.svg?branch=master">![Travis][badge.Travis]</a>
<a target="_blank" href="https://ci.appveyor.com/project/yangacer/sqlite3cpp">![Appveyor][badge.Appveyor]</a>
<a target="_blank" href="https://coveralls.io/github/yangacer/sqlite3cpp?branch=master">![Coveralls][badge.Coveralls]</a>

A C++ wrapper library for the awsome sqlite3.

> **Disclaimer**: It doesn't and won't provide any kind of ORM.

## Source

https://github.com/yangacer/sqlite3cpp

## Get Started

Require cmake at least version 3.0.


### Install
```shell
git clone https://github.com/yangacer/sqlite3cpp
cd sqlite3cpp
mkdir build
cd build
cmake .. # -DCMAKE_BUILD_TYPE=Release
make
sudo make install
```

### Hello, World!

> Compile with: g++ -std=c++11 -o hello hello.cpp -lsqlite3cpp -lsqlite3

```cpp
#include <iostream>
#include "sqlite3cpp.h"

int main() {
    using namespace sqlite3cpp;
    using std::string;

    database db(":memory:");

    cursor c = db.make_cursor();

    c.executescript(
      "create table T (msg TEXT, num INTEGER);"
      "insert into T values('Hello, World!', 1);"
      "insert into T values('Hello, sqlite3cpp!', 2);"
      );

    char const *query = "select msg from T where msg like ? and num > ?";
    for(auto const &row : c.execute(query, "%World%", 0) {
        string msg;
        int num;

        std::tie(msg, num) = row.to<string, int>();
        std::cout << "msg: " << msg << " num: " << num << std::endl;
    }

    // Output: msg: Hello, World! num: 1
}

```

## Features


- Query with range for-loop and typed parameter binding

```cpp
char const *query = "select * from T where a > ? and a < ? and b like ?";
string pattern = "test%";

for(auto const &row : c.execute(query, 1, 3, pattern)) {
    int a; std::string b;
    std::tie(a, b) = row.to<int, std::string>();

    // do something with a or b
}

```

- Create SQL scalar function with C++ lambda


```cpp
using namespace sqlite3cpp;

database db(":memory:");

auto csr = db.make_cursor();

// Create table and insert some data
csr.executescript(
  "begin;"
  "create table T (a INT, b TEXT);"
  "insert into T values(1, 'test1');"
  "insert into T values(2, 'test2');"
  "insert into T values(2, 'abc');"
  "insert into T values(3, 'test3');"
  "commit;"
  );

// Create a `mutiply` scalar function with lambda
db.create_scalar("mutiply", [](int x, int y) {
    return x * y;
});

char const *query = "select a, mutiply(a,a) from T;";

for(auto const &row : c.execute(query)) {
    int a, b;
    std::tie(a, b) = row.to<int, int>();

    // ...
}
```

- Create SQL aggregate with functor

```cpp
// Create a `stdev` aggregate
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


db.create_aggregate<stdev>("stdev");

// Invoke the stdev aggregate in SQL
char const *query = "select stdev(a) from T";
for(auto const &row : csr.execute(query)) {
    double a;
    std::tie(a) = row.to<double>();
    // EXPECT_DOUBLE_EQ(0.81649658092772603, a);
}
```

## Usage Mapping


| sqlite3         | sqlite3cpp
| ---             | ---
| sqlite3 *       | database
| sqlite3_stmt *  | cursor, row


For mapped classes of sqlite3cpp, you can obtain underlying sqlite3 struct for
inoking sqlite3 native functions.
e.g.

```cpp
database db(":memory:");

sqlite3 *db_impl = db.get();

// take the native `sqlite3_changes` for example
printf("Modified rows: %d", sqlite3_changes(db_impl, ...));

cursor c = db.make_cursor();

c.execute("select * from T");

sqlite3_stmt *stmt = c.get();

// Print column names
for(int i = 0; i < sqlite3_column_count(stmt); ++i) {
    printf("%s\t", sqlite3_column_name(stmt, i));
}
printf("\n");

for(auto const &row : c) {
    printf("%d\t", sqlite3_column_bytes(row.get()));
    // Note: row.get() also returns `sqlite3_stmt *`

}

```
> **WARNING**: Lifetime of sqlite3_stmt * ends when the associated cursor
> reaching end of results (SQLITE_DONE). Consequntly the pointer becomes a
> dangling one. Please use it with caution.

<!-- links -->
[badge.Travis]: https://travis-ci.org/yangacer/sqlite3cpp
[badge.Appveyor]: https://ci.appveyor.com/api/projects/status/yangacer/sqlite3cpp?svg=true&branch=master
[badge.Coveralls]: https://coveralls.io/repos/yangacer/sqlite3cpp/badge.svg?branch=master&service=github

