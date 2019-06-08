/*****************************************************************************
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
#include <type_traits>
#include <utility>

namespace sqlite3cpp {
namespace detail {

template <int>
struct placeholder_tmpl {};

template<int... Ints>
using indexes = std::integer_sequence<int, Ints...>;

template<int Max>
using make_indexes_t = std::make_integer_sequence<int, Max>;
}
}  // namespace sqlite3cpp::detail

// custome placeholders
namespace std {
template <int N>
struct is_placeholder<sqlite3cpp::detail::placeholder_tmpl<N>>
    : integral_constant<int, N + 1> {};
}

namespace sqlite3cpp {
namespace detail {

/**
 * tuple enumerate
 */
template <class F, class Tuple, int... I>
constexpr decltype(auto) enumerate_impl(F &&f, Tuple &&t,
                                        std::integer_sequence<int, I...>) {
  return (
      std::invoke(std::forward<F>(f), I, std::get<I>(std::forward<Tuple>(t))),
      ...);
}

template <class F, class Tuple>
constexpr decltype(auto) enumerate(F &&f, Tuple &&t) {
  return detail::enumerate_impl(
      std::forward<F>(f), std::forward<Tuple>(t),
      std::make_integer_sequence<
          int, std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
}

/**
 * Helpers for retrieve column values.
 */
inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *, int index,
                            int &val) {
  val = sqlite3_column_int(stmt, index);
}

inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *, int index,
                            int64_t &val) {
  val = sqlite3_column_int64(stmt, index);
}

inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *, int index,
                            double &val) {
  val = sqlite3_column_double(stmt, index);
}

inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *db, int index,
                            std::string &val) {
  char const* res = (char const *)sqlite3_column_text(stmt, index);
  if (!res) {
    int ec = sqlite3_errcode(db);
    if (ec != SQLITE_OK)
      throw error(ec);
  }
  val.assign(res, sqlite3_column_bytes(stmt, index));
}

inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *db, int index,
                            std::string_view &val) {
  char const *res = (char const *)sqlite3_column_text(stmt, index);
  if (!res) {
    int ec = sqlite3_errcode(db);
    if (ec != SQLITE_OK)
      throw error(ec);
  }
  val = std::string_view{
      (char const *)sqlite3_column_text(stmt, index),
      (std::string_view::size_type)sqlite3_column_bytes(stmt, index)};
}

inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *db, int index,
                            std::optional<std::string> &val) {
  char const* res = (char const *)sqlite3_column_text(stmt, index);
  if (!res) {
    int ec = sqlite3_errcode(db);
    if (ec != SQLITE_OK)
      val.reset();
  }
  val = std::string{res,
                    (std::string::size_type)sqlite3_column_bytes(stmt, index)};
}

inline void get_col_val_aux(sqlite3_stmt *stmt, sqlite3 *db, int index,
                            std::optional<std::string_view> &val) {
  char const *res = (char const *)sqlite3_column_text(stmt, index);
  if (!res) {
    int ec = sqlite3_errcode(db);
    if (ec != SQLITE_OK)
      val.reset();
  }
  val = std::string_view{
      res, (std::string_view::size_type)sqlite3_column_bytes(stmt, index)};
}

/*
 * Helpers for binding values to sqlite3_stmt.
 */
inline void bind_to_stmt(sqlite3_stmt *stmt, int i) {}

inline int bind_val(sqlite3_stmt *stmt, int index, int val) {
  return sqlite3_bind_int(stmt, index, val);
}

inline int bind_val(sqlite3_stmt *stmt, int index, double val) {
  return sqlite3_bind_double(stmt, index, val);
}

inline int bind_val(sqlite3_stmt *stmt, int index, std::string const &val) {
  return sqlite3_bind_text(stmt, index, val.c_str(), val.size(), SQLITE_STATIC);
}

inline int bind_val(sqlite3_stmt *stmt, int index, std::string_view const &val) {
  return sqlite3_bind_text(stmt, index, val.data(), val.size(), SQLITE_STATIC);
}

inline int bind_val(sqlite3_stmt *stmt, int index, char const *val) {
  return sqlite3_bind_text(stmt, index, val, -1, SQLITE_STATIC);
}

inline int bind_val(sqlite3_stmt *stmt, int index, std::nullptr_t _) {
  return sqlite3_bind_null(stmt, index);
}

template <typename T, typename... Args>
void bind_to_stmt(sqlite3_stmt *stmt, int index, T &&val, Args &&... args) {
  int ec = 0;
  if (0 != (ec = bind_val(stmt, index, std::forward<T>(val)))) throw error(ec);
  bind_to_stmt(stmt, index + 1, std::forward<Args>(args)...);
}

/**
 * Helpers for converting value from sqlite3_value.
 */
template <typename T>
struct Type {};

inline int get(Type<int>, sqlite3_value **v, int const index) {
  return sqlite3_value_int(v[index]);
}

inline int64_t get(Type<int64_t>, sqlite3_value **v, int const index) {
  return sqlite3_value_int64(v[index]);
}

inline double get(Type<double>, sqlite3_value **v, int const index) {
  return sqlite3_value_double(v[index]);
}

inline std::string get(Type<std::string>, sqlite3_value **v, int const index) {
  return std::string((char const *)sqlite3_value_text(v[index]),
                     (size_t)sqlite3_value_bytes(v[index]));
}

inline std::string_view get(Type<std::string_view>, sqlite3_value **v, int const index) {
  return std::string_view((char const *)sqlite3_value_text(v[index]),
                          (size_t)sqlite3_value_bytes(v[index]));
}

/**
 * Helpers for setting result of scalar functions.
 */
inline void result(int val, sqlite3_context *ctx) {
  sqlite3_result_int(ctx, val);
}

inline void result(int64_t val, sqlite3_context *ctx) {
  sqlite3_result_int64(ctx, val);
}

inline void result(double val, sqlite3_context *ctx) {
  sqlite3_result_double(ctx, val);
}

inline void result(std::string const &val, sqlite3_context *ctx) {
  sqlite3_result_text(ctx, val.c_str(), val.size(), SQLITE_TRANSIENT);
}

/**
 * Magic for typesafe invoking lambda/std::function from sqlite3
 * (registered via sqlite3_create_function).
 */
template <typename R, typename... Args, int... Is>
R invoke(std::function<R(Args...)> func, int argc, sqlite3_value **argv,
         indexes<Is...>) {
  // TODO Check argc
  // Expand argv per index
  return func(get(Type<Args>{}, argv, Is)...);
}

template <typename R, typename... Args>
R invoke(std::function<R(Args...)> func, int argc, sqlite3_value **argv) {
  return invoke(func, argc, argv, make_indexes_t<sizeof...(Args)>{});
}

template <typename R, typename... Args>
database::xfunc_t make_invoker(std::function<R(Args...)> &&func) {
  return [func](sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    result(invoke(func, argc, argv), ctx);
  };
}

template <typename... Args>
database::xfunc_t make_invoker(std::function<void(Args...)> &&func) {
  return [func](sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    invoke(func, argc, argv);
  };
}

/**
 * Function traits for supporting lambda.
 */

// For generic types that are functors, delegate to its 'operator()'
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

// for pointers to member function
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> {
  typedef std::function<R(Args...)> f_type;
  static const size_t arity = sizeof...(Args);
};

// for pointers to const member function
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> {
  typedef std::function<R(Args...)> f_type;
  static const size_t arity = sizeof...(Args);
};

// for function pointers
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> {
  typedef std::function<R(Args...)> f_type;
  static const size_t arity = sizeof...(Args);
};

/**
 * Member function binder helpers (auto expand by passed function prototype)
 */
template <typename F, typename C, int... Is>
typename function_traits<F>::f_type bind_this(F f, C *this_, indexes<Is...>) {
  return std::bind(f, this_, placeholder_tmpl<Is>{}...);
}

template <typename F, typename C>
typename function_traits<F>::f_type bind_this(F f, C *this_) {
  using traits = function_traits<F>;
  return bind_this(f, this_, make_indexes_t<traits::arity>{});
}
}
}  // namespace sqlite3cpp::detail

namespace sqlite3cpp {

/**
 * row impl
 */
template <typename... Cols>
std::tuple<Cols...> row::to() const {
  // TODO: Report errors
  std::tuple<Cols...> result;
  detail::enumerate(
      [this](int index, auto &&... tuple_value) {
        (detail::get_col_val_aux(m_stmt, m_db, index, tuple_value), ...);
      },
      result);
  return result;
}

/**
 * cursor impl
 */
template <typename... Args>
cursor &cursor::execute(std::string const &sql, Args &&... args) {
  sqlite3_stmt *stmt = 0;
  int ec = 0;

  if (0 != (ec = sqlite3_prepare_v2(m_db, sql.c_str(), sql.size(), &stmt, 0)))
    throw error(ec);

  m_stmt.reset(stmt);
  detail::bind_to_stmt(m_stmt.get(), 1, std::forward<Args>(args)...);
  step();
  return *this;
}

/**
 * database impl
 */

template <typename... Args>
cursor database::execute(std::string const &sql, Args &&... args) {
  cursor c = make_cursor();
  c.execute(sql, std::forward<Args>(args)...);
  return c;
}

template <typename FUNC>
void database::create_scalar(std::string const &name, FUNC func, int flags) {
  using traits = detail::function_traits<FUNC>;

  auto *xfunc_ptr =
      new xfunc_t(detail::make_invoker(typename traits::f_type(func)));

  int ec = 0;
  if (0 != (ec = sqlite3_create_function_v2(
                m_db.get(), name.c_str(), (int)traits::arity, flags,
                (void *)xfunc_ptr, &database::forward, 0, 0, &dispose))) {
    delete xfunc_ptr;
    throw error(ec);
  }
}

template <typename AG>
void database::create_aggregate(std::string const &name, int flags) {
  using detail::make_invoker;
  using detail::bind_this;
  using detail::result;
  using traits = detail::function_traits<decltype(&AG::step)>;

  aggregate_wrapper_t *wrapper = new aggregate_wrapper_t;
  AG *inst = new AG;
  wrapper->reset = [inst]() { *inst = AG(); };
  wrapper->release = [inst]() { delete inst; };
  wrapper->step = make_invoker(bind_this(&AG::step, inst));
  wrapper->fin = [inst](sqlite3_context *ctx) {
    result(inst->finalize(), ctx);
  };

  int ec = 0;
  if (0 != (ec = sqlite3_create_function_v2(
                m_db.get(), name.c_str(), (int)traits::arity, flags,
                (void *)wrapper, 0, &step_ag, &final_ag, &dispose_ag))) {
    delete inst;
    delete wrapper;
    throw error(ec);
  }
}

}  // namespace sqlite3cpp
