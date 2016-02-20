#include "sqlite3cpp.h"
#include <cassert>
#include <stdexcept>

namespace sqlite3cpp {

row::row(cursor &csr)
    :m_stmt(csr.get())
{}

row_iter &row_iter::operator++()
{ m_csr.commit(); return *this; }

bool row_iter::operator ==(row_iter const &i) const
{ return &m_csr == &i.m_csr && m_csr.get() == nullptr; }

bool row_iter::operator !=(row_iter const &i) const
{ return !(this->operator==(i)); }

cursor::cursor(database const &db)
: m_db(db.get())
{}


database::database(std::string const &urn)
{
    sqlite3 *i = 0;
    if(sqlite3_open(urn.c_str(), &i)) {
        assert(i == 0);
        throw std::runtime_error("open database failure");
    }
    m_instance.reset(i);
}

cursor database::make_cursor() const
{
    return cursor(*this);
}

}

