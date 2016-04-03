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
          "create table T (msg TEXT);"
          );

        auto cnt = data_size >> 4;
        for(size_t i = 0; i < cnt; ++i) {
            c.executescript("insert into T values(strftime('%Y-%m-%d %H:%M:%f', 'now'))");
        }
    };
}

std::function<void()> seq_scan(int index, int argc, char **argv)
{

}

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
            "-r <pattern>\tScan testdata with specified pattern (seq|rand).",
            seq_scan
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
