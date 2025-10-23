#pragma once

#include <dbi.h>

using namespace Tools;

const unsigned USER_NAME_LEN=20;
const unsigned PASSWORD_LEN=50;
const unsigned TIME_LEN=19;
const unsigned NAME_LEN=20;
const unsigned FILE_LEN=1024;
const unsigned MAC_ADDRESS_LEN=17;
const unsigned IP_ADDRESS_LEN=15;
const unsigned ACTION_LEN=50;

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
  enum class HIST_TYPE {
	HIST_AN,
	HIST_AE,
	HIST_LO
  };



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

  void setHist( HIST_TYPE hist_type, const std::string & user = std::string() );
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

class P_PLAY_QUEUE_CHUNKS : public PLAY_QUEUE_CHUNKS
{
 public:
  P_PLAY_QUEUE_CHUNKS();

  P_PLAY_QUEUE_CHUNKS & operator=( const PLAY_QUEUE_CHUNKS & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class PLAY_QUEUE_MUSIC : public PLAY_QUEUE_CHUNKS
{
 public:
  PLAY_QUEUE_MUSIC();
};

class P_PLAY_QUEUE_MUSIC : public PLAY_QUEUE_MUSIC
{
public:
	P_PLAY_QUEUE_MUSIC();

	P_PLAY_QUEUE_MUSIC & operator=( const PLAY_QUEUE_MUSIC & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

// TIME=650922;SEQ=161;MAC=D8:BC:38:FA:EF:20;IP=192.168.1.137;ACTION=ButtonReleased
class BUTTON_QUEUE : public BASE
{
public:
	DBTypeInt		time_stamp;
	DBTypeInt		seq;
	DBTypeVarChar 	mac_address;
	DBTypeVarChar	ip_address;
	DBTypeVarChar	action;

public:
	BUTTON_QUEUE();
	BUTTON_QUEUE( const BUTTON_QUEUE & other )
	: BUTTON_QUEUE()
	{
		BASE::operator=(other);
	}

	BUTTON_QUEUE & operator=( const BUTTON_QUEUE & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class P_BUTTON_QUEUE : public BUTTON_QUEUE
{
public:
	P_BUTTON_QUEUE();

	P_BUTTON_QUEUE & operator=( const BUTTON_QUEUE & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class USER : public BASE
{
public:
	DBTypeVarChar	username;
	DBTypeVarChar	password;
	DBTypeVarChar 	button_mac_address;
	DBTypeVarChar	home_directory;
	DBTypeVarChar	button_press_event0;
	DBTypeVarChar	button_press_event1;
	DBTypeVarChar	button_press_event2;
	DBTypeVarChar	button_press_event3;
	DBTypeVarChar	button_press_event4;
	DBTypeVarChar	play_chunk0;
	DBTypeVarChar	play_chunk1;
	DBTypeVarChar	play_chunk2;
	DBTypeVarChar	play_chunk3;
	DBTypeVarChar	play_chunk4;


public:
	USER();

	USER & operator=( const USER & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

std::string create_sql();
std::string create_sql_statement( DBBindType *table );

