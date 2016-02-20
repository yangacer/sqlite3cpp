#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <iostream>
#include <list>

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

namespace sqlval2cpp {

using cb_t = std::function<void(sqlite3_context*, sqlite3_value **)>;
template<typename T> struct Type{};

inline int get(Type<int>, sqlite3_value** v, int const index)
{ return sqlite3_value_int(v[index]); }

inline std::string get(Type<std::string>, sqlite3_value **v, int const index)
{ return std::string((char const *)sqlite3_value_text(v[index]), (size_t)sqlite3_value_bytes(v[index])); }

inline void result(int val, sqlite3_context *ctx)
{ sqlite3_result_int(ctx, val); }

inline void result(std::string const &val, sqlite3_context *ctx)
{ sqlite3_result_text(ctx, val.c_str(), val.size(), SQLITE_TRANSIENT); }

template<int...> struct indexes { typedef indexes type; };
template<int Max, int...Is> struct make_indexes : make_indexes<Max-1, Max-1, Is...>{};
template<int... Is> struct make_indexes<0, Is...> : indexes<Is...>{};
template<int Max> using make_indexes_t=typename make_indexes<Max>::type;
 
template<typename R, typename ...Args, int ...Is>
R invoke(std::function<R(Args...)> func, sqlite3_value **argv, indexes<Is...>) {
    return func( get(Type<Args>{}, argv, Is)... ); 
}

template<typename R, typename... Args>
R invoke( std::function<R(Args...)> func, sqlite3_value **argv) {
    return invoke(func, argv, make_indexes_t<sizeof...(Args)>{} );
}

// For generic types that are functors, delegate to its 'operator()'
template <typename T>
struct function_traits
    : public function_traits<decltype(&T::operator())>
{};

// for pointers to member function
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const> {
    typedef std::function<ReturnType (Args...)> f_type;
    static const size_t arity = sizeof...(Args);
};


template<typename R, typename ... Args>
cb_t make_invoker(std::function<R(Args...)> func)
{
    return [func](sqlite3_context *ctx, sqlite3_value **argv) {
        result(invoke(func, argv), ctx);
    };
}
} // namespace sqlval2cpp

struct database
{
    database(std::string const &urn);
    cursor make_cursor() const;
    sqlite3 *get() const { return m_instance.get(); }

    template<typename FUNC>
    void create_scalar(std::string const &name, FUNC func)
    {
        using namespace sqlval2cpp;
        using traits = function_traits<FUNC>;
        typename traits::f_type stdfunc(func);

        m_scalars.push_back(make_invoker(stdfunc));
        sqlite3_create_function(
          m_instance.get(),
          name.c_str(),
          (int)traits::arity,
          SQLITE_UTF8,
          (void*)&m_scalars.back(),
          &database::forward,
          0, 0);
    }

private:
    static void forward(sqlite3_context *ctx, int argc, sqlite3_value **argv)
    {
        auto *cb = (sqlval2cpp::cb_t*)sqlite3_user_data(ctx);
        (*cb)(ctx, argv);
    }

    std::unique_ptr<sqlite3, database_deleter> m_instance;
    std::list<sqlval2cpp::cb_t> m_scalars;
};

} // namespace sqlite3cpp
