/*
 * db.h
 *
 *  Created on: 10/04/2013
 *      Author: nicholas
 */

#ifndef DB_H_
#define DB_H_
#include "sqlite3.h"
#include <stdexcept>
#include <string>
#include <tuple>

template<int...>
struct seq{};

template<int N, int ...S>
struct gens : gens<N - 1, N - 1, S...> {};

template<int ...S>
struct gens<0, S...>
{
	typedef seq<S...> type;
};

class db_t
{
private:
	sqlite3* db;
public:
	struct error : std::runtime_error
	{
		int code;
		error(const std::string& what, int code);
	};

	template<typename... Args>
	class statement_t
	{
	private:
		sqlite3_stmt* stmt;
		db_t& db;

		void bind_arg_int(int arg, double val)
		{
	    	if(int res = sqlite3_bind_double(stmt, arg, val) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}
		void bind_arg_int(int arg, int val)
		{
	    	if(int res = sqlite3_bind_int(stmt, arg, val) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}
		void bind_arg_int(int arg, uint64_t val)
		{
	    	if(int res = sqlite3_bind_int64(stmt, arg, val) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}
		void bind_arg_int(int arg, sqlite3_int64 val)
		{
	    	if(int res = sqlite3_bind_int64(stmt, arg, val) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}
		void bind_arg_int(int arg, std::nullptr_t val)
		{
	    	if(int res = sqlite3_bind_null(stmt, arg) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}
		void bind_arg_int(int arg, const std::string& val)
		{
	    	if(int res = sqlite3_bind_text(stmt, arg, val.c_str(), val.size(), SQLITE_TRANSIENT) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}

		void bind_arg(int)
		{
		}

		template <typename T, typename... Tail>
	    void bind_arg(int arg, T val, const Tail &... args)
	    {
			bind_arg_int(arg, val);
	    	bind_arg(arg+1, args...);
	    }

	    void unpack_column_int(int col, double& val)
	    {
	    	val = sqlite3_column_double(stmt, col);
	    }
	    void unpack_column_int(int col, int& val)
	    {
	    	val = sqlite3_column_int(stmt, col);
	    }
	    void unpack_column_int(int col, sqlite3_int64& val)
	    {
	    	val = sqlite3_column_int64(stmt, col);
	    }
	    void unpack_column_int(int col, std::string& val)
	    {
	    	auto c = sqlite3_column_text(stmt, col);
	    	const std::string::size_type n = sqlite3_column_bytes(stmt, col);
	    	val = std::string(c, c+n);
	    }

	    void unpack_column(int)
	    {
	    }

	    template <typename T, typename... Tail>
	    void unpack_column(int col, T& val, Tail&... args)
	    {
	    	unpack_column_int(col, val);
	    	unpack_column(col+1, args...);
	    }

		template<typename... Tail, int ...S>
		void unpack_row(std::tuple<Tail...>& params, seq<S...>)
		{
			unpack_column(0, std::get<S>(params)...);
		}

	public:
		statement_t(const statement_t&) = delete;
		statement_t& operator=(const statement_t&) = delete;

		statement_t(db_t& db, const std::string& sql)
		 : stmt(nullptr), db(db)
		{
			if(int res = sqlite3_prepare_v2(db, sql.c_str(), sql.size(), &stmt, nullptr) != SQLITE_OK)
				throw error{sqlite3_errmsg(db), res};
		}

		template <typename Fn, typename... Res>
		void query(Fn func, const Args &... args)
		{
			bind_arg(1, args...);

	        while(true)
	        {
	            int res = sqlite3_step(stmt);

	            if(res == SQLITE_DONE)
	            	break;

	            if(res != SQLITE_ROW)
	            	throw error{sqlite3_errmsg(db), res};

	            if(sizeof...(Res) > static_cast<size_t>(sqlite3_column_count(stmt)))
	            	throw error{"Record column count mismatch", 0};

	            std::tuple<Res...> row;

	            unpack_row(row, typename gens<sizeof...(Res)>::type());
	            func(row);
	        }

			sqlite3_reset(stmt);
		}

		void execute(const Args &... args)
		{
			bind_arg(1, args...);

			int res = sqlite3_step(stmt);

			if(res != SQLITE_ROW && res != SQLITE_DONE)
				throw error{sqlite3_errmsg(db), res};

			sqlite3_reset(stmt);
		}

		~statement_t()
		{
			sqlite3_finalize(stmt);
		}
	};

	db_t(const std::string& filename);

	db_t(const db_t&) = delete;
	db_t& operator=(const db_t&) = delete;

	void execute(const std::string& sql);

	operator sqlite3*() const;

	~db_t();
};

#endif /* DB_H_ */
