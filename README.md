# sqlite3cpp

[![Build Status](https://travis-ci.org/yangacer/sqlite3cpp.svg?branch=master)](https://travis-ci.org/yangacer/sqlite3cpp)
<!--
[![Coverage Status](https://coveralls.io/repos/yangacer/sqlite3cpp/badge.svg?branch=master&service=github)](https://coveralls.io/github/yangacer/sqlite3cpp?branch=master) 
-->

## Source

https://github.com/yangacer/sqlite3cpp

## Get Started

Require cmake at least version 3.0.

```shell
git clone https://github.com/yangacer/sqlite3cpp
cd sqlite3cpp
mkdir build
cd build
cmake .. # -DCMAKE_BUILD_TYPE=Release
make
sudo make install
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
