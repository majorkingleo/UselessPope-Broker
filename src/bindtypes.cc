#include "bindtypes.h"

std::string now_string()
{
  time_t t = time( NULL );
  struct tm * tm = localtime( &t );

  return format( "%d-%02d-%02d %02d:%02d:%02d",
                 tm->tm_year + 1900,
                 tm->tm_mon + 1,
                 tm->tm_mday,
                 tm->tm_hour,
                 tm->tm_min,
                 tm->tm_sec );
}

const std::string JANEIN::STYPES[] = { "NEIN", "JA" };

Forkey::Forkey( DBBindType *base, 
		  DBType *field_,
		  const std::string & target_table_,
		  const std::string & target_field_ )
  : own_table( base->get_table() ),
	field( field_->get_name() ),
	target_table( target_table_ ),
	target_field( target_field_ )
{}

BASE::BASE( const std::string & name, DBBindType *base )
  : DBBindType( name ),
	idx( base, "idx" ),
	hist_an_zeit( base, "hist_an_zeit" ),
	hist_an_user( base, "hist_an_user", USER_NAME_LEN ),
	hist_ae_zeit( base, "hist_ae_zeit" ),
	hist_ae_user( base, "hist_ae_user", USER_NAME_LEN ),
	hist_lo_zeit( base, "hist_lo_zeit" ),
	hist_lo_user( base, "hist_lo_user", USER_NAME_LEN )
{

}

void BASE::setHist( HIST_TYPE hist_type, const std::string & user )
{
  std::string db_time;
  struct tm *tm;
  time_t t = time(NULL);

  tm = localtime( & t );

  db_time = format( "%d-%02d-%02d %02d:%02d:%02d",
                    tm->tm_year + 1900,
                    tm->tm_mon + 1,
                    tm->tm_mday,
                    tm->tm_hour,
                    tm->tm_min,
                    tm->tm_sec );

  switch( hist_type )
  {
  using enum HIST_TYPE;
  case HIST_AN:
	  hist_an_zeit = db_time;
	  hist_an_user = user;
	  break;
  case HIST_AE:
	  hist_ae_zeit = db_time;
	  hist_ae_user = user;
	  break;
  case HIST_LO:
	  hist_lo_zeit = db_time;
	  hist_lo_user = user;
	  break;
  }
}

PLAY_QUEUE_CHUNKS::PLAY_QUEUE_CHUNKS()
  : BASE( "PLAY_QUEUE_CHUNKS", this ),
	file( this, "file", FILE_LEN )
{}

P_PLAY_QUEUE_CHUNKS::P_PLAY_QUEUE_CHUNKS()
: PLAY_QUEUE_CHUNKS(),
  sermon_reaction_idx( this, "sermon_reaction_idx" )
{
	set_table_name( "P_PLAY_QUEUE_CHUNKS" );
}

PLAY_QUEUE_MUSIC::PLAY_QUEUE_MUSIC()
  : PLAY_QUEUE_CHUNKS()
{
	set_table_name( "PLAY_QUEUE_MUSIC" );
}

P_PLAY_QUEUE_MUSIC::P_PLAY_QUEUE_MUSIC()
: PLAY_QUEUE_MUSIC()
{
	set_table_name( "P_PLAY_QUEUE_MUSIC" );
}

BUTTON_QUEUE::BUTTON_QUEUE()
: BASE( "BUTTON_QUEUE", this ),
  time_stamp( this, "time_stamp" ),
  seq( this, "seq" ),
  mac_address( this, "mac_address", MAC_ADDRESS_LEN ),
  ip_address( this, "ip_address", IP_ADDRESS_LEN ),
  action( this, "action", ACTION_LEN ),
  file( this, "file", FILE_LEN )
{
	//add_key( new Forkey( this, &mac_address, "USER", "button_mac_address" ) );
}

P_BUTTON_QUEUE::P_BUTTON_QUEUE()
: BUTTON_QUEUE()
{
	set_table_name( "P_BUTTON_QUEUE" );
}


USERS_ACTION::USERS_ACTION()
: BASE( "USERS_ACTIONS", this ),
  username( this, "username", USER_NAME_LEN ),
  button_mac_address( this, "button_mac_address", MAC_ADDRESS_LEN ),
  home_directory( this, "home_directory" ),
  button_press_event0( this, "button_press_event0", ACTION_LEN ),
  button_press_event1( this, "button_press_event1", ACTION_LEN ),
  button_press_event2( this, "button_press_event2", ACTION_LEN ),
  button_press_event3( this, "button_press_event3", ACTION_LEN ),
  button_press_event4( this, "button_press_event4", ACTION_LEN ),
  play_chunk0( this, "play_chunk0", FILE_LEN ),
  play_chunk1( this, "play_chunk1", FILE_LEN ),
  play_chunk2( this, "play_chunk2", FILE_LEN ),
  play_chunk3( this, "play_chunk3", FILE_LEN ),
  play_chunk4( this, "play_chunk4", FILE_LEN )
{
	add_key( new Forkey( this, &username, "USERS", "username" ) );
}

CONFIG::CONFIG()
: BASE( "CONFIG", this ),
  key( this, "key", CONFIG_KEY_LEN ),
  value( this, "value", CONFIG_VALUE_LEN )
{

}

STATS::STATS()
: BASE( "STATS", this ),
  key( this, "key", CONFIG_KEY_LEN ),
  value( this, "value", CONFIG_VALUE_LEN )
{

}


PLAY_QUEUE_ANIMATION::PLAY_QUEUE_ANIMATION()
  : PLAY_QUEUE_CHUNKS()
{
	set_table_name( "PLAY_QUEUE_ANIMATION" );
}

P_PLAY_QUEUE_ANIMATION::P_PLAY_QUEUE_ANIMATION()
  : PLAY_QUEUE_ANIMATION()
{
	set_table_name( "P_PLAY_QUEUE_ANIMATION" );
}

SERMON::SERMON()
: BASE( "SERMON", this ),
  action( this, "action", SERMON_ACTION_LEN ),
  reaction( this, "reaction", SERMON_REACTION_LEN )
{

}

BF1942_SERVER::BF1942_SERVER()
: BASE( "BF1942_SERVER", this ),
  name( this, "name", BF1942_SERVER_NAME_LEN ),
  guid( this, "guid", BF1942_GUID_LEN ),
  ip_address( this, "ip_address", IP_ADDRESS_LEN ),
  last_seen( this, "last_seen" )
{

}

BF1942_STATUS::BF1942_STATUS()
: BASE( "BF1942_STATUS", this ),
  server_ip( this, "server_ip", IP_ADDRESS_LEN ),
  hostname( this, "hostname", BF1942_HOSTNAME_LEN ),
  mapname( this, "mapname", BF1942_MAPNAME_LEN ),
  gametype( this, "gametype", BF1942_GAMETYPE_LEN ),
  hostport( this, "hostport" ),
  numplayers( this, "numplayers" ),
  maxplayers( this, "maxplayers" ),
  round_time_remain( this, "round_time_remain" ),
  tickets1( this, "tickets1" ),
  tickets2( this, "tickets2" ),
  status( this, "status" ),
  round_started_at( this, "round_started_at" ),
  last_seen( this, "last_seen" )
{

}

BF1942_PLAYER::BF1942_PLAYER()
: BASE( "BF1942_PLAYER", this ),
  server_ip( this, "server_ip", IP_ADDRESS_LEN ),
  player_name( this, "player_name", BF1942_PLAYER_NAME_LEN ),
  slot( this, "slot" ),
  score( this, "score" ),
  kills( this, "kills" ),
  deaths( this, "deaths" ),
  ping( this, "ping" ),
  team( this, "team" ),
  last_seen( this, "last_seen" )
{

}

BF1942_GAME::BF1942_GAME()
: BASE( "BF1942_GAME", this ),
  server_ip( this, "server_ip", IP_ADDRESS_LEN ),
  mapname( this, "mapname", BF1942_MAPNAME_LEN ),
  gametype( this, "gametype", BF1942_GAMETYPE_LEN ),
  started_at( this, "started_at" ),
  ended_at( this, "ended_at" ),
  winning_team( this, "winning_team" ),
  tickets1( this, "tickets1" ),
  tickets2( this, "tickets2" ),
  victory_type( this, "victory_type", BF1942_VICTORY_TYPE_LEN )
{

}

P_BF1942_PLAYER::P_BF1942_PLAYER()
: BF1942_PLAYER(),
  game_id( this, "game_id" ),
  mapname( this, "mapname", BF1942_MAPNAME_LEN ),
  winning_team( this, "winning_team" )
{
	set_table_name( "P_BF1942_PLAYER" );
}

static std::string create_sql_statement( DBBindType *table, std::vector< Ref<Forkey> > & forkeys, bool add_drop_table )
{
  std::string s;

  if( add_drop_table ) {
  	s = "DROP TABLE IF EXISTS `" + table->get_table() + "`;\n";    	
  }

  s += "CREATE TABLE IF NOT EXISTS `" + table->get_table() + '`';
  s += " (\n ";
  
  std::vector<DBType*> tl = table->get_types();

  for( unsigned i = 0; i < tl.size(); i++ ) 
	{
	  if( i > 0 ) 
		{
		  s += ",\n";
		}

	  s += "`" + tl[i]->get_name() + "` ";

	  switch( tl[i]->type ) 
		{
		case DBType::TYPE::INT:
		  s += "INT ";
		  break;

		case DBType::TYPE::DOUBLE:
		  s += "DOUBLE ";
		  break;

		case DBType::TYPE::VARCHAR:
		  s += Tools::format("VARCHAR(%s) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''", tl[i]->get_size() );
		  break;

		case DBType::TYPE::ENUM:
		  {
			s += "ENUM ( ";
			
			DBTypeEnumBase *eb = dynamic_cast<DBTypeEnumBase*>( tl[i] );
			
			if( eb ) 
			  {
				for( unsigned j = eb->get_first_case(); j <= eb->get_last_case(); j++ )
				  {
					if( j > eb->get_first_case() )
					  s += ", ";
					
					s += Tools::format( "'%s'", eb->get_case(j));
				  }			  
			  }
			
			s += " ) CHARACTER SET utf8 COLLATE utf8_general_ci ";
		  }
		  break;

		case DBType::TYPE::DATETIME:
		  s += "DATETIME DEFAULT current_timestamp()";
		  break;

		case DBType::TYPE::FIRST__:
		case DBType::TYPE::LAST__:
		  break;
		}

	  s += "NOT NULL ";
	}

  std::string engine = "INNODB";

  TableType *tt = dynamic_cast<TableType*>(table);

  if( tt )
	{
	  switch( tt->table_type )
		{
		case TableType::MYISAM: 
		  engine = "MYISAM";
		  break;
		case TableType::INNODB:
		  engine = "INNODB";
		  break;
		}
	}

  s += Tools::format( "\n) ENGINE = %s;\n", engine );
  s += Tools::format("ALTER TABLE `%s` ADD PRIMARY KEY IF NOT EXISTS ( `idx` );\n",table->get_table());
  s += Tools::format("ALTER TABLE `%s` CHANGE `idx` `idx` INT( 11 ) NOT NULL AUTO_INCREMENT;\n",
			  table->get_table());
  s += Tools::format("ALTER TABLE `%s`  DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;\n",table->get_table());

  Forkeys *fk = dynamic_cast<Forkeys*>( table );

  if( fk )
	{
	  forkeys.insert( forkeys.end(), fk->forkeys.begin(), fk->forkeys.end() );
	}
  
  return s;
}

std::string create_sql( bool add_drop_table )
{
  std::string s;
  PLAY_QUEUE_CHUNKS 		play_queue_chunks;
  PLAY_QUEUE_MUSIC 			play_queue_music;
  PLAY_QUEUE_ANIMATION 		play_queue_animation;
  BUTTON_QUEUE				button_queue;
  USERS_ACTION				users_action;
  CONFIG					config;
  STATS						stats;
  SERMON					sermon;
  BF1942_SERVER				bf1942_server;
  BF1942_STATUS				bf1942_status;
  BF1942_PLAYER				bf1942_player;
  BF1942_GAME				bf1942_game;
  P_BF1942_PLAYER			p_bf1942_player;

  P_PLAY_QUEUE_CHUNKS 		p_play_queue_chunks;
  P_PLAY_QUEUE_MUSIC 		p_play_queue_music;
  P_PLAY_QUEUE_ANIMATION 	p_play_queue_animation;
  P_BUTTON_QUEUE			p_button_queue;

  std::vector< Ref<Forkey> > forkeys;

  s += create_sql_statement( &play_queue_chunks, 		forkeys, add_drop_table );
  s += create_sql_statement( &play_queue_music,  		forkeys, add_drop_table );
  s += create_sql_statement( &play_queue_animation,		forkeys, add_drop_table );
  s += create_sql_statement( &button_queue,				forkeys, add_drop_table );
  s += create_sql_statement( &users_action,				forkeys, add_drop_table );
  s += create_sql_statement( &config,					forkeys, add_drop_table );
  s += create_sql_statement( &stats,					forkeys, add_drop_table );
  s += create_sql_statement( &sermon,					forkeys, add_drop_table );
  s += create_sql_statement( &bf1942_server,			forkeys, add_drop_table );
  s += create_sql_statement( &bf1942_status,			forkeys, add_drop_table );
  s += create_sql_statement( &bf1942_player,			forkeys, add_drop_table );
  s += create_sql_statement( &bf1942_game,			forkeys, add_drop_table );
  s += create_sql_statement( &p_bf1942_player,		forkeys, add_drop_table );
  s += "ALTER TABLE `BF1942_SERVER` ADD INDEX IF NOT EXISTS `idx_BF1942_SERVER_guid`(`guid`);\n";
  s += "ALTER TABLE `BF1942_STATUS` ADD INDEX IF NOT EXISTS `idx_BF1942_STATUS_server_ip`(`server_ip`);\n";
  s += "ALTER TABLE `BF1942_PLAYER` ADD INDEX IF NOT EXISTS `idx_BF1942_PLAYER_server_player`(`server_ip`,`player_name`);\n";
  s += "ALTER TABLE `BF1942_GAME` ADD INDEX IF NOT EXISTS `idx_BF1942_GAME_server_ip`(`server_ip`);\n";
  s += "ALTER TABLE `P_BF1942_PLAYER` ADD INDEX IF NOT EXISTS `idx_P_BF1942_PLAYER_game`(`game_id`);\n";
  s += "ALTER TABLE `P_BF1942_PLAYER` ADD INDEX IF NOT EXISTS `idx_P_BF1942_PLAYER_game_player`(`game_id`,`player_name`);\n";

  s += create_sql_statement( &p_play_queue_chunks, 		forkeys, add_drop_table );
  s += create_sql_statement( &p_play_queue_music,		forkeys, add_drop_table );
  s += create_sql_statement( &p_play_queue_animation,	forkeys, add_drop_table );
  s += create_sql_statement( &p_button_queue,			forkeys, add_drop_table );

  // notwendige indexe anlegen
  for( unsigned i = 0; i < forkeys.size(); i++ )
	{
	  if( forkeys[i]->target_field != "idx" ) 
		{
		  std::string ss;

		  ss = Tools::format( "ALTER TABLE `%s` ADD INDEX IF NOT EXISTS `%s_%s_%s`(`%s`);\n",
					   forkeys[i]->target_table,
					   "idx",
					   forkeys[i]->target_table,
					   forkeys[i]->target_field,
					   forkeys[i]->target_field );

		  // Damit wir nicht 2 mal den selben index anlegen. 
		  if( s.find( ss ) == std::string::npos )
			{
			  s += ss;
			}
		}
	}

  // Forkeys anlegen
  for( unsigned i = 0; i < forkeys.size(); i++ )
	{
	  s += Tools::format( "ALTER TABLE `%s` add FOREIGN KEY IF NOT EXISTS (`%s`) REFERENCES `%s`(`%s`);\n",
				   forkeys[i]->own_table,
				   forkeys[i]->field,
				   forkeys[i]->target_table,
				   forkeys[i]->target_field );
	}

  return s;
}

