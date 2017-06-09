/*****************************************************************************
 * Copyright (c) 2017, Acer Yun-Tse Yang All rights reserved
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
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include "sqlite3cpp.h"

std::function<void()> gen_test_data(int index, int argc, char **argv) {
  if (index + 1 >= argc) throw std::invalid_argument("missing data size (mb)");

  size_t data_size = strtoul(argv[index + 1], 0, 10) << 20;

  return [data_size]() {
    sqlite3cpp::database db("testdata.db");

    auto c = db.make_cursor();

    c.executescript(
        "pragma journal_mode=wal;"
        "drop table if exists T;"
        "create table T (msg TEXT, rand INTEGER);");

    auto cnt = data_size >> 4;
    for (size_t i = 0; i < cnt; ++i) {
      c.executescript(
          "insert into T values(strftime('%Y-%m-%d %H:%M:%f', 'now'), "
          "random())");
    }
  };
}

template <typename T>
struct scan {
  static void sequential() {
    sqlite3cpp::database db("testdata.db");

    auto c = db.make_cursor();
    size_t cnt = 0;
    T ts;

    for (auto const &row : c.execute("select msg from T")) {
      std::tie(ts) = row.to<T>();
      cnt += 1;
    }

    std::cout << "scan " << cnt << " rows" << std::endl;
  }

  static void random() {
    sqlite3cpp::database db("testdata.db");

    auto c = db.make_cursor();
    size_t cnt = 0;
    T ts;

    for (auto const &row : c.execute("select msg from T order by rand")) {
      std::tie(ts) = row.to<T>();
      cnt += 1;
    }

    std::cout << "scan " << cnt << " rows" << std::endl;
  }

  std::function<void()> operator()(int index, int argc, char **argv) const {
    if (index + 1 >= argc)
      throw std::invalid_argument("missing scan pattern (seq|rand)");

    char const *pattern = argv[index + 1];

    if (!strcmp("seq", pattern)) {
      return scan::sequential;
    } else if (!strcmp("rand", pattern)) {
      return scan::random;
    } else {
      throw std::invalid_argument("invalid scan pattern");
    }
  }
};

int main(int argc, char **argv) {
  using std::function;
  using opt_act_t = function<function<void()>(int idx, int argc, char **argv)>;

  struct option {
    char const *opt;
    char const *cmt;
    opt_act_t act;
  } options[] = {
      {"-g", "-g <mb>\tGenerate testdata.db of specified size.", gen_test_data},
      {"-rc",
       "-rc <seq|rand>\tScan testdata with specified pattern (sequential or "
       "random) in copy semantic.",
       scan<std::string>()},
      {"-rr",
       "-rr <seq|rand>\tScan testdata with specified pattern (sequential or "
       "random) in ref semantic.",
       scan<sqlite3cpp::string_ref>()},
      {"-h", "-h\tPrint usage.", {}}};

  opt_act_t help = [&options](int, int, char **) {

    return [&options]() {
      using namespace std;
      cout << "Usage:" << endl;
      for (auto const &op : options) {
        cout << "\t" << op.cmt << endl;
      }
    };
  };

  options[sizeof(options) / sizeof(option) - 1].act = help;

  for (int i = 1; i < argc; ++i) {
    for (auto const &op : options) {
      if (strcmp(argv[i], op.opt)) {
        continue;
      }
      op.act(i, argc, argv)();
    }
  }

  return 0;
}
