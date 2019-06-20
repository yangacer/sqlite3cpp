/*****************************************************************************
 * The BSD 3-Clause License
 *
 * Copyright (c) 2019, Acer Yun-Tse Yang All rights reserved.
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
#include "version.h"

#if !defined(NDEBUG)
#include <cstdio>
#define DBG(...) printf(__VA_ARGS__)
#endif

namespace sqlite3cpp {

namespace detail {
// An tag type for row iter session
struct session {};
}  // namespace detail

/**
 * row_iter impl
 */
row_iter::row_iter(cursor &csr, std::weak_ptr<void> session) noexcept
    : m_csr(&csr), m_session(std::move(session)) {
  if (m_session.expired())
    m_csr = nullptr;
  else {
    m_row.m_stmt = m_csr->get();
    m_row.m_db = m_csr->m_db;
  }
}

row_iter &row_iter::operator++() {
  if (!m_session.expired()) m_csr->step();
  if (m_session.expired()) m_csr = nullptr;
  return *this;
}

row const &row_iter::operator*() const noexcept {
  assert(is_valid());
  return m_row;
}

row const *row_iter::operator->() const noexcept {
  assert(is_valid());
  return &m_row;
}

bool row_iter::operator==(row_iter const &i) const noexcept {
  return m_session.lock() == i.m_session.lock();
}

bool row_iter::operator!=(row_iter const &i) const noexcept {
  return !(*this == i);
}

bool row_iter::is_valid() const noexcept { return !m_session.expired(); }
/**
 * cursor impl
 */
cursor::cursor(database const &db) noexcept : m_db(db.get()) {}

cursor &cursor::executescript(std::string const &sql) {
  int ec = 0;
  if (0 != (ec = sqlite3_exec(m_db, sql.c_str(), 0, 0, 0))) throw error(ec);
  return *this;
}

void cursor::step() {
  assert(m_stmt && "null cursor");

  int ec = sqlite3_step(m_stmt.get());

  switch (ec) {
    case SQLITE_DONE:
      m_session.reset();
      break;
    case SQLITE_ROW:
      break;
    default:
      throw error(ec);
  }
}

row_iter cursor::begin() noexcept {
  if (!m_stmt) return {};
  // NOTE(acer): There is actually a redundant reset as we invoke
  // |execute().begin()|. It's possible to be eliminated, though I keep it for
  // ensuring non-query SQL can be executed right away after calling
  // |execute()|. Besides, results of previous |step()| should be cached by
  // sqlite3 s.t. performance penality would be minor.
  m_session.reset((void *)new detail::session,
                  [](void *s) { delete (detail::session *)s; });
  sqlite3_reset(m_stmt.get());
  step();
  return row_iter(*this, m_session);
}

row_iter cursor::end() noexcept { return {}; }
/**
 * transaction impl
 */

transaction::transaction(database &db, transaction::params_t const &params)
    : m_db(db), m_params(params) {
  m_db.execute(m_params.begin_sql);
}

transaction::transaction(database &db) : transaction(db, {}) {}

transaction::~transaction() {
  try {
    m_db.execute(m_params.end_sql);
  } catch (...) {
  }
}

void transaction::commit() noexcept { m_params.end_sql = "end"; }

/**
 * database impl
 */
database::database(std::string const &urn) {
  sqlite3 *i = 0;
  int ec = 0;

  if (0 != (ec = sqlite3_open(urn.c_str(), &i))) throw error(ec);

  m_db.reset(i);
}

cursor database::make_cursor() const noexcept { return cursor(*this); }

std::string database::version() const { return SQLITE3CPP_VERSION_STRING; }

cursor database::executescript(std::string const &sql) {
  cursor c = make_cursor();
  c.executescript(sql);
  return c;
}

void database::forward(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  auto *cb = (xfunc_t *)sqlite3_user_data(ctx);
  assert(cb != 0);

  try {
    (*cb)(ctx, argc, argv);
  } catch (std::bad_alloc const &) {
    sqlite3_result_error_nomem(ctx);
  } catch (...) {
    sqlite3_result_error_code(ctx, SQLITE_ABORT);
  }
}

void database::dispose(void *user_data) {
  auto *cb = (xfunc_t *)user_data;
  assert(cb != 0);

  delete cb;
}

void database::step_ag(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
  auto *wrapper = (aggregate_wrapper_t *)sqlite3_user_data(ctx);
  assert(wrapper != 0);

  try {
    wrapper->step(ctx, argc, argv);
  } catch (std::bad_alloc const &) {
    sqlite3_result_error_nomem(ctx);
  } catch (...) {
    sqlite3_result_error_code(ctx, SQLITE_ABORT);
  }
}

void database::final_ag(sqlite3_context *ctx) {
  auto *wrapper = (aggregate_wrapper_t *)sqlite3_user_data(ctx);
  assert(wrapper != 0);

  try {
    wrapper->fin(ctx);
    wrapper->reset();
  } catch (std::bad_alloc const &) {
    sqlite3_result_error_nomem(ctx);
  } catch (...) {
    sqlite3_result_error_code(ctx, SQLITE_ABORT);
  }
}

void database::dispose_ag(void *user_data) {
  auto *wrapper = (aggregate_wrapper_t *)user_data;
  assert(wrapper != 0);

  wrapper->release();
  delete wrapper;
}
}  // namespace sqlite3cpp
