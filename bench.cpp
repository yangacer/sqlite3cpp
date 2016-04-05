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


struct scan {

    static void sequential() {
        sqlite3cpp::database db("testdata.db");

        auto c = db.make_cursor();
        size_t cnt = 0;
        std::string ts;

        for (auto const & row : c.execute("select msg from T")) {
            std::tie(ts) = row.to<std::string>();
            cnt += 1;
        }

        std::cout << "scan " << cnt << " rows" << std::endl;
    }

    static void random() {
        sqlite3cpp::database db("testdata.db");

        auto c = db.make_cursor();
        size_t cnt = 0;
        std::string ts;

        for (auto const & row : c.execute("select msg from T order by rand")) {
            std::tie(ts) = row.to<std::string>();
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

    struct opt {
        char const *opt;
        char const *cmt;
        opt_act_t act;
    } options[] = {
        {
            "-g",
            "-g <mb>\tGenerate testdata.db of specified size.",
            gen_test_data
        }, {
            "-r",
            "-r <seq|rand>\tScan testdata with specified pattern (sequential or random).",
            scan()
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
            for(int j=0; j < sizeof(options)/sizeof(opt); ++j) {
                cout << "\t" << options[j].cmt << endl;
            }
        };
    };

    options[2].act = help;

    for(int i=1; i < argc; ++i) {
        for(int j=0; j < sizeof(options)/sizeof(opt); ++j) {
            if (strcmp(argv[i], options[j].opt)) {
                continue;
            }
            options[j].act(i, argc, argv)();
        }
    }

    return 0;
}
