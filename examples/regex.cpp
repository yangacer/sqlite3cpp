#include <regex>
#include <string>
#include <sstream>
#include <iostream>

#include "sqlite3cpp.h"


int main() {
    using namespace sqlite3cpp;

    database db(":memory:");

    auto re_replace = [](std::string pattern,
                         std::string value,
                         std::string text)
    {
        // Replace regex |pattern| found in |text| with |value|
        std::stringstream out;
        std::regex re(pattern);

        std::regex_replace(std::ostreambuf_iterator<char>(out),
                           text.begin(), text.end(),
                           re, value);
        return out.str();
    };

    // Register the lambda
    db.create_scalar("re_replace", re_replace);

    // Test data
    auto csr = db.make_cursor();
    csr.executescript(
      "CREATE TABLE T (data TEXT);"
      "INSERT INTO T VALUES('Quick brown fox');"
      );

    // Replace vowels with '*'
    char const *query = "SELECT re_replace('a|e|i|o|u', '*', data) FROM T";

    // Execute the query and print out replaced results
    for(auto const &row : csr.execute(query)) {
        string_ref result;
        std::tie(result) = row.to<string_ref>();
        std::cout << result << std::endl;
    }

    // Should print:
    // Q**ck br*wn f*x

    return 0;
}
