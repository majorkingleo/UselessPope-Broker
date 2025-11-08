#pragma once
#include "db.h"

#ifdef TOOLS_USE_DB

#include <thread>
#include <map>
#include <memory>
#include <mutex>

namespace Tools {

class Database
{
 private:
  DB *db = nullptr;
  std::string sql;
  std::string err;

  Database() {}
  Database( const Database &db_ ) {}

 public:
  
#ifdef TOOLS_USE_MYSQL
  static const unsigned DB_MYSQL;
#endif

#ifdef TOOLS_USE_ORACLE
  static const unsigned DB_ORACLE;
#endif

#ifdef TOOLS_USE_ODBC
  static const unsigned DB_ODBC;
#endif

 public:
  Database( const std::string &host, 
	    const std::string &user, 
	    const std::string &passwd, 
	    const std::string &instance, unsigned type );
  ~Database();

  bool operator!() const { return !db; }
  bool valid() const { return db != NULL; }

  std::string get_error() const { return db ? db->error() : err; }

  DBErg<DBRowList> exec( const std::string &query )
    {
      sql = query;
      return db->query( query );
    }

  DBErg<DBRowList> select( const std::string &query, bool table_names = true )
    {
      sql = query;
      return db->select( query, table_names );
    }

  void rollback()
  {
    exec( "rollback;" );
  }

  void commit()
  {
    exec( "commit;" );
  }

  std::string get_sql() const { return sql; }
  int  get_insert_id() { return db->insert_id(); }

  DBErg<DBRowList> select( const std::string &table, const DBRow &which, const std::string &extra ); 
  DBErg<DBRowList> insert( const std::string &table, const DBRow &row );
  DBErg<DBRowList> insert( const std::string &table, const DBRowList &rows );

  DBErg<DBRowList> update( const std::string &table, const DBRow &row, const std::string extra = std::string() );

 private:
  std::string create_values_list( const std::string &table, const std::vector<std::string> &names );
};

class ThreadedDatabase
{
public:
	class DisposeToken
	{
	private:
		 ThreadedDatabase *m_db;

	public:
		 DisposeToken() = delete;

		 DisposeToken( ThreadedDatabase & db )
		 : m_db( &db )
		 {}

		 DisposeToken( DisposeToken && other )
		 : m_db( other.m_db )
		 {
			 other.m_db = nullptr;
		 }

		 ~DisposeToken()
		 {
			 if( m_db ) {
				 m_db->dispose();
			 }
		 }
	};

private:
	std::string m_host;
	std::string m_user;
	std::string m_passwd;
	std::string m_instance;
	unsigned 	m_type {};

	std::map<std::thread::id,std::shared_ptr<Database>> m_instances;

	std::recursive_mutex m_tex {};

public:
	ThreadedDatabase() = default;
	ThreadedDatabase( const ThreadedDatabase & other ) = delete;

	ThreadedDatabase( const std::string & host,
		    		  const std::string & user,
					  const std::string & passwd,
					  const std::string & instance,
					  unsigned type )
	: m_host( host ),
	  m_user( user ),
	  m_passwd( passwd ),
	  m_instance( instance ),
	  m_type( type )
	{}

	ThreadedDatabase & operator=( const ThreadedDatabase & other ) = delete;

	void connect( const std::string & host,
		    		  const std::string & user,
					  const std::string & passwd,
					  const std::string & instance,
					  unsigned type )
	{
		auto lock = std::scoped_lock( m_tex );

		m_host = host;
		m_user = user;
		m_passwd = passwd;
		m_instance = instance;
		m_type = type;

		m_instances.clear();
	}

	bool operator!();

	Database & operator*() {
		return *at();
	}

	std::shared_ptr<Database> & operator->() {
		return at();
	}

	std::shared_ptr<Database> & at();

	void dispose();

	DisposeToken get_dispose_token() {
		return DisposeToken( *this );
	}
};

} // namespace Tools

#endif

