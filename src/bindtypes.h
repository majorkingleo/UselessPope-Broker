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
const unsigned BF1942_GUID_LEN=64;
const unsigned BF1942_SERVER_NAME_LEN=128;
const unsigned BF1942_HOSTNAME_LEN=128;
const unsigned BF1942_MAPNAME_LEN=64;
const unsigned BF1942_GAMETYPE_LEN=32;
const unsigned BF1942_PLAYER_NAME_LEN=64;
const unsigned ACTION_LEN=50;
const unsigned CONFIG_KEY_LEN=50;
const unsigned CONFIG_VALUE_LEN=FILE_LEN;
const unsigned SERMON_ACTION_LEN = 1024;
const unsigned SERMON_REACTION_LEN = 1024;

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
  DBTypeInt	sermon_reaction_idx;

 public:
  P_PLAY_QUEUE_CHUNKS();

  P_PLAY_QUEUE_CHUNKS( const P_PLAY_QUEUE_CHUNKS & b )
  : P_PLAY_QUEUE_CHUNKS()
  {
	  BASE::operator=(b);
  }

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

class PLAY_QUEUE_ANIMATION : public PLAY_QUEUE_CHUNKS
{
 public:
	PLAY_QUEUE_ANIMATION();

	PLAY_QUEUE_ANIMATION & operator=( const PLAY_QUEUE_ANIMATION & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class P_PLAY_QUEUE_ANIMATION : public PLAY_QUEUE_ANIMATION
{
 public:
	P_PLAY_QUEUE_ANIMATION();

	P_PLAY_QUEUE_ANIMATION & operator=( const PLAY_QUEUE_ANIMATION & b )
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
	DBTypeVarChar	file;

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

	P_BUTTON_QUEUE( const BUTTON_QUEUE & b )
	: P_BUTTON_QUEUE()
	{
		BASE::operator=(b);
	}

	P_BUTTON_QUEUE & operator=( const BUTTON_QUEUE & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class USERS_ACTION : public BASE
{
public:
	DBTypeVarChar	username;
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
	USERS_ACTION();
	USERS_ACTION( const USERS_ACTION & b )
	: USERS_ACTION()
	{
		BASE::operator=(b);
	}

	USERS_ACTION & operator=( const USERS_ACTION & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};


class CONFIG : public BASE
{
public:
	DBTypeVarChar	key;
	DBTypeVarChar	value;

public:
	CONFIG();
	CONFIG( const CONFIG & b )
	: CONFIG()
	{
		BASE::operator=(b);
	}

	CONFIG & operator=( const CONFIG & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class STATS : public BASE
{
public:
	DBTypeVarChar	key;
	DBTypeVarChar	value;

public:
	STATS();
	STATS( const STATS & b )
	: STATS()
	{
		BASE::operator=(b);
	}

	STATS & operator=( const STATS & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class SERMON : public BASE
{
public:
	DBTypeVarChar	action;
	DBTypeVarChar	reaction;

public:
	SERMON();
	SERMON( const SERMON & b )
	: SERMON()
	{
		BASE::operator=(b);
	}

	SERMON & operator=( const SERMON & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

std::string create_sql( bool add_drop_table = false );

// Current local time in the "YYYY-MM-DD HH:MM:SS" format the DATETIME columns
// (and BASE::setHist) use.
std::string now_string();

class BF1942_SERVER : public BASE
{
public:
	DBTypeVarChar	name;		// server name (beacon byte 1..NUL)
	DBTypeVarChar	guid;		// "{...}" identifier (unique key)
	DBTypeVarChar	ip_address;	// source IP of the beacon
	DBTypeDateTime	last_seen;	// most recent beacon timestamp

public:
	BF1942_SERVER();
	BF1942_SERVER( const BF1942_SERVER & b )
	: BF1942_SERVER()
	{
		BASE::operator=(b);
	}

	BF1942_SERVER & operator=( const BF1942_SERVER & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class BF1942_STATUS : public BASE
{
public:
	DBTypeVarChar	server_ip;	// query target IP (join key)
	DBTypeVarChar	hostname;
	DBTypeVarChar	mapname;
	DBTypeVarChar	gametype;
	DBTypeInt		hostport;
	DBTypeInt		numplayers;
	DBTypeInt		maxplayers;
	DBTypeDateTime	last_seen;

public:
	BF1942_STATUS();
	BF1942_STATUS( const BF1942_STATUS & b )
	: BF1942_STATUS()
	{
		BASE::operator=(b);
	}

	BF1942_STATUS & operator=( const BF1942_STATUS & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

class BF1942_PLAYER : public BASE
{
public:
	DBTypeVarChar	server_ip;	// join key to BF1942_STATUS / BF1942_SERVER
	DBTypeVarChar	player_name;
	DBTypeInt		slot;		// scoreboard index <i>
	DBTypeInt		score;		// may be negative
	DBTypeInt		kills;
	DBTypeInt		deaths;
	DBTypeInt		ping;
	DBTypeInt		team;
	DBTypeDateTime	last_seen;

public:
	BF1942_PLAYER();
	BF1942_PLAYER( const BF1942_PLAYER & b )
	: BF1942_PLAYER()
	{
		BASE::operator=(b);
	}

	BF1942_PLAYER & operator=( const BF1942_PLAYER & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};

