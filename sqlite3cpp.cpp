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
#include "sqlite3cpp.h"
#include <cassert>
#include <stdexcept>

namespace sqlite3cpp {


char const *error::what() const noexcept {
    return sqlite3_errstr(code);
}

/**
 * row_iter impl
 */
row_iter::row_iter(cursor &csr) noexcept
    : m_csr(&csr) 
{
    if(!m_csr->get())
        m_csr = nullptr;
    else
        m_row.m_stmt = m_csr->get();
}

row_iter &row_iter::operator++() {
    m_csr->step();
    if(!m_csr->get()) m_csr = nullptr;
    return *this;
}

bool row_iter::operator == (row_iter const &i) const noexcept
{ return m_csr == i.m_csr; }

bool row_iter::operator !=(row_iter const &i) const noexcept
{ return !(*this == i); }

/**
 * cursor impl
 */
cursor::cursor(database const &db) noexcept
: m_db(db.get())
{}

cursor &cursor::executescript(std::string const &sql)
{
    int ec = 0;
    if(0 != (ec = sqlite3_exec(m_db, sql.c_str(), 0, 0, 0)))
       throw error(ec);
    return *this;
}

void cursor::step() {
    assert(m_stmt && "null cursor");

    int ec = sqlite3_step(m_stmt.get());

    switch(ec) {
    case SQLITE_DONE:
        m_stmt.reset();
        break;
    case SQLITE_ROW:
        break;
    default:
        throw error(ec);
    }
}

/**
 * database impl
 */
database::database(std::string const &urn)
{
    sqlite3 *i = 0;
    int ec = 0;

    if(0 != (ec = sqlite3_open(urn.c_str(), &i)))
        throw error(ec);

    m_db.reset(i);
}

cursor database::make_cursor() const noexcept
{
    return cursor(*this);
}

void database::forward(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    auto *cb = (xfunc_t*)sqlite3_user_data(ctx);
    (*cb)(ctx, argc, argv);
}

void database::dispose(void *user_data) {
    auto *cb = (xfunc_t*)user_data;
    delete cb;
}

void database::step_ag(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    auto *wrapper = (aggregate_wrapper_t*)sqlite3_user_data(ctx);
    wrapper->step(ctx, argc, argv);
}

void database::final_ag(sqlite3_context *ctx) {
    auto *wrapper = (aggregate_wrapper_t*)sqlite3_user_data(ctx);
    wrapper->fin(ctx);
    wrapper->reset();
}

void database::dispose_ag(void *user_data) {
    auto *wrapper = (aggregate_wrapper_t*)user_data;
    wrapper->release();
    delete wrapper;
}
} // namespace sqlite3cpp

