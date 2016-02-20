#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <list>

extern "C" {
#include "sqlite3.h" 
}
namespace sqlite3cpp {

/**
 * Forwarded decls
 */
struct database;
struct cursor;
struct row_iter;
struct row;

struct database_deleter {
    void operator()(sqlite3 *i) const;
};

struct statement_deleter {
    void operator()(sqlite3_stmt *s) const;
};

struct row
{
    template<typename ... Cols>
    std::tuple<Cols...> get() const;
private:
    friend struct row_iter;
    row(cursor &csr);
    sqlite3_stmt *m_stmt;
};

struct row_iter
{
    row_iter &operator++();
    bool operator == (row_iter const &i) const;
    bool operator != (row_iter const &i) const;
    row operator*() const { return row(m_csr); }
private:
    friend struct cursor;
    row_iter(cursor &csr) : m_csr(csr) {}
    cursor &m_csr;
};

struct cursor
{
    template<typename ... Args>
    cursor& execute(std::string const &sql, Args&& ... args);

    void executescript(std::string const &sql);

    row_iter begin() { step(); return row_iter(*this); }
    row_iter end()   { return row_iter(*this); }

    sqlite3_stmt *get() const { return m_stmt.get(); }

private:
    void step();
    friend struct row_iter;
    friend struct database;
    cursor(database const &db);
    sqlite3 *m_db;
    std::unique_ptr<sqlite3_stmt, statement_deleter> m_stmt;
};

struct database
{
    using scalar_callback_t = std::function<void(sqlite3_context*,
                                                 sqlite3_value **)>;

    database(std::string const &urn);
    cursor make_cursor() const;
    sqlite3 *get() const { return m_instance.get(); }

    template<typename FUNC>
    void create_scalar(std::string const &name, FUNC func);

private:
    static void forward(sqlite3_context *ctx, int argc, sqlite3_value **argv)
    {
        auto *cb = (scalar_callback_t*)sqlite3_user_data(ctx);
        (*cb)(ctx, argv);
    }

    std::unique_ptr<sqlite3, database_deleter> m_instance;
    std::list<scalar_callback_t> m_scalars;
};

} // namespace sqlite3cpp


#include "sqlite3cpp.ipp"
