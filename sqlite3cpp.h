#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <iostream>

extern "C" {
#include "sqlite3.h" 
}
namespace sqlite3cpp {

struct database_deleter {
    void operator()(sqlite3 *i) const
    {
        if (sqlite3_close(i))
            throw std::runtime_error("close database failure");
    }
};

struct statement_deleter {
    void operator()(sqlite3_stmt *s) const
    {
        if(sqlite3_finalize(s))
            throw std::runtime_error("finalize database failed");
    }
};


void get_col_val(sqlite3_stmt *stmt, int index, int& val)
{ val = sqlite3_column_int(stmt, index); }

void get_col_val(sqlite3_stmt *stmt, int index, std::string &val)
{ val = (char const *)sqlite3_column_text(stmt, index); }

/**
 * tuple foreach
 */
template<typename tuple_type, typename F, int Index, int Max>
struct foreach_tuple_impl {
    void operator()(tuple_type & t, F f) {
        f(std::get<Index>(t), Index);
        foreach_tuple_impl<tuple_type, F, Index + 1, Max>()(t, f);
    }
};

template<typename tuple_type, typename F, int Max>
struct foreach_tuple_impl<tuple_type, F, Max, Max> {
    void operator()(tuple_type & t, F f) {
        f(std::get<Max>(t), Max);
    }
};

template<typename tuple_type, typename F>
void foreach_tuple_element(tuple_type & t, F f)
{
    foreach_tuple_impl<
        tuple_type,
        F,
        0,
        std::tuple_size<tuple_type>::value - 1>()(t, f);
}

struct set_col_val {
    set_col_val(sqlite3_stmt *stmt) : m_stmt(stmt) {}

    template<typename T>
    void operator()(T &out, int index) const
    { get_col_val(m_stmt, index, out); }
private:
    sqlite3_stmt *m_stmt;
};

struct cursor;
struct row_iter;
struct row
{
    template<typename ... Cols>
    std::tuple<Cols...> get() const {
        std::tuple<Cols ...> result;
        foreach_tuple_element(
          result,
          set_col_val(m_stmt)
          );
        return result;
    }
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
    row_iter(cursor &csr)
    : m_csr(csr)
    {}
    cursor &m_csr;
};

/*
 * Bind helpers
 */
void bind_(sqlite3_stmt *stmt, int i){}

int bind_val(sqlite3_stmt *stmt, int index, int val) {
    return sqlite3_bind_int(stmt, index, val);
}

int bind_val(sqlite3_stmt *stmt, int index, std::string const &val) {
    return sqlite3_bind_text(stmt, index, val.c_str(), val.size(), SQLITE_STATIC);
}

template <typename T, typename ... Args>
void bind_(sqlite3_stmt *stmt, int index, T val, Args&& ... args)
{
    if(bind_val(stmt, index, val))
        throw std::runtime_error("bind error");
    bind_(stmt, index+1, std::forward<Args>(args)...);
}

struct database; // forwarded decl
struct cursor
{
    template<typename ... Args>
    cursor& execute(std::string const &sql, Args&& ... args)
    {
        sqlite3_stmt *stmt = 0;
        if(sqlite3_prepare_v2(m_db, sql.c_str(), sql.size(), &stmt, 0))
        {
            throw std::runtime_error("execute statement failure");
        }
        m_stmt.reset(stmt);
        bind_(m_stmt.get(), 1, std::forward<Args>(args)...);
        return *this;
    }

    void executescript(std::string const &sql)
    {
        sqlite3_exec(m_db, sql.c_str(), 0, 0, 0);
    }

    row_iter begin() { commit(); return row_iter(*this); }
    row_iter end()   { return row_iter(*this); }

    void commit() {
        if (!m_stmt) throw std::runtime_error("null cursor");
        switch(sqlite3_step(m_stmt.get())) {
        case SQLITE_DONE:
            m_stmt.reset();
            break;
        case SQLITE_ROW:
            break;
        default:
            throw std::runtime_error("advance cursor failure");
        }
    }

    sqlite3_stmt *get() const { return m_stmt.get(); }
private:
    friend class database;
    cursor(database const &db);
    sqlite3 *m_db;
    std::unique_ptr<sqlite3_stmt, statement_deleter> m_stmt;
};

struct database
{
    database(std::string const &urn);
    cursor make_cursor() const;
    sqlite3 *get() const { return m_instance.get(); }
private:
    std::unique_ptr<sqlite3, database_deleter> m_instance;
};

}
