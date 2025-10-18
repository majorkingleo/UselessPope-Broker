#pragma once

#include <dbi.h>

using namespace Tools;

const unsigned USER_NAME_LEN=20;
const unsigned TIME_LEN=19;
const unsigned NAME_LEN=20;
const unsigned FILE_LEN=1024;

struct JANEIN
{
  enum ETYPE
	{
	  FIRST__=-1,
	  NEIN=0,
	  JA=1,
	  LAST__
	};
  static const std::string STYPES[];
};

template< class T > std::ostream & operator<<( std::ostream & out, const Tools::EnumRange<T> & jn )
{
  if( jn.value <= jn.FIRST__ || jn.value >= jn.LAST__ )
	return out << jn.STYPES[0];
  else
	return out << jn.STYPES[jn.value];
}

template< class T > std::istream & operator>>( std::istream & in, Tools::EnumRange<T> & jn )
{
  std::string s;

  in >> s;

  for( int i = jn.FIRST__ + 1; i < jn.LAST__; i++ )
	{
	  if( s == jn.STYPES[i] ) 
		{
		  jn.value = static_cast<JANEIN::ETYPE>(i);
		  break;
		}
	}
  return in;
}


template<class T> bool is_bool( const Tools::EnumRange<T> & t ) { return false; }

class TableType
{
 public:
  enum Type
	{
	  MYISAM,
	  INNODB
	};

  Type table_type;

 public:
  TableType( Type type = INNODB )
	: table_type( type )
	{}
  virtual ~TableType() {}
};

class Forkey
{
 public:
  const std::string own_table;
  const std::string field;
  const std::string target_table;
  const std::string target_field;
  
  Forkey( DBBindType *base, 
		  DBType *field_,
		  const std::string & target_table_,
		  const std::string & target_field_ );		  

  virtual ~Forkey() {}
};

class Forkeys
{
 public:
  std::vector< Ref<Forkey> > forkeys;

  void add_key( Forkey *key )
	{
	  forkeys.push_back( key );
	}
};

class BASE : public DBBindType, public TableType, public Forkeys
{
 public:
  DBTypeInt      idx;
  DBTypeDateTime hist_an_zeit;
  DBTypeVarChar  hist_an_user;
  DBTypeDateTime hist_ae_zeit;
  DBTypeVarChar  hist_ae_user;  
  DBTypeDateTime hist_lo_zeit;
  DBTypeVarChar  hist_lo_user;  

  BASE( const std::string & name, DBBindType *base );

  BASE & operator=( const BASE & b )
	{ 
	  DBBindType::operator=(b); 
	  return *this;
	}
};

class PLAY_QUEUE_CHUNKS : public BASE
{
 public:
  DBTypeVarChar file;
  
  PLAY_QUEUE_CHUNKS();

  PLAY_QUEUE_CHUNKS & operator=( const PLAY_QUEUE_CHUNKS & b )
	{  
	  BASE::operator=(b); 
	  return *this;
	}
};

std::string create_sql();
std::string create_sql_statement( DBBindType *table );

