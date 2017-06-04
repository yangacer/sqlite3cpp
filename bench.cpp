#include <fstream>
#include <iostream>
#include <functional>
#include <cstring>
#include "sqlite3cpp.h"

std::function<void()> gen_test_data(int index, int argc, char **argv)
{
    if (index + 1 >= argc)
        throw std::invalid_argument("missing data size (mb)");

    size_t data_size = strtoul(argv[index+1], 0, 10) << 20;

    return [data_size](){
        sqlite3cpp::database db("testdata.db");

        auto c = db.make_cursor();

        c.executescript(
          "pragma journal_mode=wal;"
          "drop table if exists T;"
          "create table T (msg TEXT, rand INTEGER);"
          );

        auto cnt = data_size >> 4;
        for(size_t i = 0; i < cnt; ++i) {
            c.executescript("insert into T values(strftime('%Y-%m-%d %H:%M:%f', 'now'), random())");
        }
    };
}


template<typename T>
struct scan {

    static void sequential() {
        sqlite3cpp::database db("testdata.db");

        auto c = db.make_cursor();
        size_t cnt = 0;
        T ts;

        for (auto const & row : c.execute("select msg from T")) {
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

        for (auto const & row : c.execute("select msg from T order by rand")) {
            std::tie(ts) = row.to<T>();
            cnt += 1;
        }

        std::cout << "scan " << cnt << " rows" << std::endl;
    }

    std::function<void()> operator()(int index, int argc, char **argv) const
    {
        if (index + 1 >= argc)
            throw std::invalid_argument("missing scan pattern (seq|rand)");

        char const *pattern = argv[index + 1];

        if (!strcmp("seq", pattern)) {
            return scan::sequential;
        } else if(!strcmp("rand", pattern)) {
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
        {
            "-g",
            "-g <mb>\tGenerate testdata.db of specified size.",
            gen_test_data
        }, {
            "-rc",
            "-rc <seq|rand>\tScan testdata with specified pattern (sequential or random) in copy semantic.",
            scan<std::string>()
        },{
            "-rr",
            "-rr <seq|rand>\tScan testdata with specified pattern (sequential or random) in ref semantic.",
            scan<sqlite3cpp::string_ref>()
        }, {
            "-h",
            "-h\tPrint usage.",
            {}
        }
    };

    opt_act_t help = [&options](int, int, char **) {

        return [&options]() {
            using namespace std;
            cout << "Usage:" << endl;
            for(auto const &op : options) {
                cout << "\t" << op.cmt << endl;
            }
        };
    };

    options[sizeof(options)/sizeof(option) - 1].act = help;

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
