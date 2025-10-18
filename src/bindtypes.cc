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
                    tm->tm_hour + 1,
                    tm->tm_min + 1,
                    tm->tm_sec + 1 );

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
  PLAY_QUEUE_CHUNKS play_queue_chunks;

  std::vector< Ref<Forkey> > forkeys;

  s += create_sql_statement( &play_queue_chunks, forkeys );

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

