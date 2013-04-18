/*
 * db.cpp
 *
 *  Created on: 10/04/2013
 *      Author: nicholas
 */

#include "db.h"

db_t::error::error(const std::string& what, int code)
 : std::runtime_error(what), code(code)
{
}

db_t::db_t(const std::string& filename)
 : db(nullptr)
{
	if(int res = sqlite3_open(filename.c_str(), &db) != SQLITE_OK)
	{
		error ex{sqlite3_errmsg(db), res};
		sqlite3_close(db);
		throw ex;
	}
}

void db_t::execute(const std::string& sql)
{
	db_t::statement_t<> stmt{*this, sql};
	stmt.execute();
}

db_t::operator sqlite3*() const
{
	return db;
}

db_t::~db_t()
{
	sqlite3_close(db);
}
