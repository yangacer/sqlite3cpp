#include "sqlite3cpp.h"
#include <cassert>
#include <stdexcept>

namespace sqlite3cpp {

/**
 * row impl
 */
row::row(cursor &csr)
    :m_stmt(csr.get())
{}

/**
 * row_iter impl
 */
row_iter::row_iter(cursor &csr) : m_csr(&csr) {
    if (!m_csr->get()) m_csr = nullptr;
}

row_iter &row_iter::operator++() {
    m_csr->step();
    if(!m_csr->get()) m_csr = nullptr;
    return *this;
}

bool row_iter::operator == (row_iter const &i) const
{ return m_csr == i.m_csr; }

bool row_iter::operator !=(row_iter const &i) const
{ return !(*this == i); }

/**
 * cursor impl
 */
cursor::cursor(database const &db)
: m_db(db.get())
{}

void cursor::executescript(std::string const &sql)
{
    sqlite3_exec(m_db, sql.c_str(), 0, 0, 0);
}

void cursor::step() {
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

/**
 * database impl
 */
database::database(std::string const &urn)
{
    sqlite3 *i = 0;
    if(sqlite3_open(urn.c_str(), &i)) {
        assert(i == 0);
        throw std::runtime_error("open database failure");
    }
    m_db.reset(i);
}

cursor database::make_cursor() const
{
    return cursor(*this);
}

void database::forward(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    auto *cb = (xfunc_t*)sqlite3_user_data(ctx);
    (*cb)(ctx, argv);
}

void database::dispose(void *user_data) {
    auto *cb = (xfunc_t*)user_data;
    delete cb;
}

void database::step_ag(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    auto *wrapper = (aggregate_wrapper_t*)sqlite3_user_data(ctx);
    wrapper->step(ctx, argv);
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

