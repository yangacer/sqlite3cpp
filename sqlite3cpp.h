/*****************************************************************************
 * The BSD 3-Clause License
 *
 * Copyright (c) 2016, Acer Yun-Tse Yang All rights reserved.
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
#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <functional>
#include <exception>
#include "stringpiece.h"
#ifdef _WIN32
#include "sqlite3cpp_export.h"
#else
#define SQLITE3CPP_EXPORT
#endif
extern "C" {
#include "sqlite3.h"
}

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

#define C_STYLE_DELETER(T, F) \
    struct T##_deleter {\
        void operator()(T* mem) const { F(mem); } \
    }

namespace sqlite3cpp {

using string_ref = re2::StringPiece;

/**
 * Forwarded decls
 */
struct error;
struct database;
struct cursor;
struct row_iter;
struct row;
struct aggregate;

C_STYLE_DELETER(sqlite3, sqlite3_close);
C_STYLE_DELETER(sqlite3_stmt, sqlite3_finalize);

struct SQLITE3CPP_EXPORT error : std::exception {
    error(int code) noexcept : code(code) {}
    char const *what() const noexcept;
    int code;
};

struct SQLITE3CPP_EXPORT row
{
    template<typename ... Cols>
    std::tuple<Cols...> to() const;
    sqlite3_stmt *get() const noexcept { return m_stmt; }
private:
    friend struct row_iter;
    row() : m_stmt(nullptr) {}
    sqlite3_stmt *m_stmt;
};

struct SQLITE3CPP_EXPORT row_iter
{
    row_iter &operator++();
    bool operator == (row_iter const &i) const noexcept;
    bool operator != (row_iter const &i) const noexcept;
    row const &operator*() const noexcept { return m_row; }
    row const *operator->() const noexcept { return &m_row; }
private:
    friend struct cursor;
    row_iter() noexcept : m_csr(nullptr) {}
    row_iter(cursor &csr) noexcept;
    cursor *m_csr;
    row m_row;
};

struct SQLITE3CPP_EXPORT cursor
{
    template<typename ... Args>
    cursor &execute(std::string const &sql, Args&& ... args);

    cursor &executescript(std::string const &sql);

    row_iter begin() noexcept { return row_iter(*this); }
    row_iter end() noexcept { return row_iter(); }

    sqlite3_stmt *get() const noexcept { return m_stmt.get(); }

private:
    void step();
    friend struct row_iter;
    friend struct database;
    cursor(database const &db) noexcept;
    sqlite3 *m_db;
    std::unique_ptr<sqlite3_stmt, sqlite3_stmt_deleter> m_stmt;
};

struct SQLITE3CPP_EXPORT database
{
    using xfunc_t = std::function<void(sqlite3_context*, int, sqlite3_value **)>;
    using xfinal_t = std::function<void(sqlite3_context*)>;
    using xreset_t = std::function<void()>;

    database(std::string const &urn);

    cursor make_cursor() const noexcept;
    sqlite3 *get() const noexcept { return m_db.get(); }

    template<typename FUNC>
    void create_scalar(std::string const &name,
                       FUNC func,
                       int flags=SQLITE_UTF8 | SQLITE_DETERMINISTIC);

    template<typename AG>
    void create_aggregate(std::string const &name,
                          int flags=SQLITE_UTF8 | SQLITE_DETERMINISTIC);

    std::string version() const;
private:

    struct aggregate_wrapper_t {
        xfunc_t step;
        xfinal_t fin;
        xreset_t reset;
        xreset_t release;
    };

    static void forward(sqlite3_context *ctx, int argc, sqlite3_value **argv);
    static void dispose(void *user_data);
    static void step_ag(sqlite3_context *ctx, int argc, sqlite3_value **argv);
    static void final_ag(sqlite3_context *ctx);
    static void dispose_ag(void *user_data);

    std::unique_ptr<sqlite3, sqlite3_deleter> m_db;
};

} // namespace sqlite3cpp


#include "sqlite3cpp.ipp"
