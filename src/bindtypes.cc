#include "bindtypes.h"

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
: PLAY_QUEUE_CHUNKS()
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
  action( this, "action", ACTION_LEN )
{
	//add_key( new Forkey( this, &mac_address, "USER", "button_mac_address" ) );
}

P_BUTTON_QUEUE::P_BUTTON_QUEUE()
: BUTTON_QUEUE()
{
	set_table_name( "P_BUTTON_QUEUE" );
}


USER::USER()
: BASE( "USER", this ),
  username( this, "username", USER_NAME_LEN ),
  password( this, "password", PASSWORD_LEN ),
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

}

std::string create_sql_statement( DBBindType *table, std::vector< Ref<Forkey> > & forkeys )
{
  std::string s;

  s = "DROP TABLE IF EXISTS `" + table->get_table() + "`;\n";
  s += "CREATE TABLE `" + table->get_table() + '`';
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
		  s += Tools::format("VARCHAR(%s) CHARACTER SET utf8 COLLATE utf8_general_ci ", tl[i]->get_size() );
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
		  s += "DATETIME ";
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
  s += Tools::format("ALTER TABLE `%s` ADD PRIMARY KEY ( `idx` );\n",table->get_table());
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

std::string create_sql()
{
  std::string s;
  PLAY_QUEUE_CHUNKS 	play_queue_chunks;
  PLAY_QUEUE_MUSIC 		play_queue_music;
  BUTTON_QUEUE			button_queue;
  USER					user;

  P_PLAY_QUEUE_CHUNKS 	p_play_queue_chunks;
  P_PLAY_QUEUE_MUSIC 	p_play_queue_music;
  P_BUTTON_QUEUE		p_button_queue;

  std::vector< Ref<Forkey> > forkeys;

  s += create_sql_statement( &play_queue_chunks, 	forkeys );
  s += create_sql_statement( &play_queue_music,  	forkeys );
  s += create_sql_statement( &button_queue,			forkeys );
  s += create_sql_statement( &user,					forkeys );

  s += create_sql_statement( &p_play_queue_chunks, 	forkeys );
  s += create_sql_statement( &p_play_queue_music,	forkeys );
  s += create_sql_statement( &p_button_queue,		forkeys );

  // notwendige indexe anlegen
  for( unsigned i = 0; i < forkeys.size(); i++ )
	{
	  if( forkeys[i]->target_field != "idx" ) 
		{
		  std::string ss;

		  ss = Tools::format( "ALTER TABLE `%s` ADD INDEX `%s_%s_%s`(`%s`);\n",
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
	  s += Tools::format( "ALTER TABLE `%s` add FOREIGN KEY (`%s`) REFERENCES `%s`(`%s`);\n",
				   forkeys[i]->own_table,
				   forkeys[i]->field,
				   forkeys[i]->target_table,
				   forkeys[i]->target_field );
	}

  return s;
}

