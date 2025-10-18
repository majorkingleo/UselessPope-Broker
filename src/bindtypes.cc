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
	gueltig( base, "gueltig" ),
	gueltig_von( base, "gueltig_von" ),
	gueltig_bis( base, "gueltig_bis" ),
	hist_an_zeit( base, "hist_an_zeit" ),
	hist_an_user( base, "hist_an_user", USER_NAME_LEN ),
	hist_ae_zeit( base, "hist_ae_zeit" ),
	hist_ae_user( base, "hist_ae_user", USER_NAME_LEN ),
	hist_lo_zeit( base, "hist_lo_zeit" ),
	hist_lo_user( base, "hist_lo_user", USER_NAME_LEN )
{
  gueltig.data = JANEIN::JA;
}

GRP_UST::GRP_UST()
  : BASE( "GRP_UST", this ),
	name( this, "name", NAME_LEN ),
	bezeichnung( this, "bezeichnung", BEZEICHNUNG_LEN ),
	bezeichnung2( this, "bezeichnung2", BEZEICHNUNG_LEN ),
	ust( this, "ust" )
{}

ART::ART()
  : BASE( "ART", this ),
	artnr( this, "artnr", NAME_LEN ),
	bezeichnung( this, "bezeichnung", BEZEICHNUNG_LEN ),
	bezeichnung2( this, "bezeichnung2", BEZEICHNUNG_LEN ),
	gruppe_ust( this, "gruppe_ust", NAME_LEN ),
	gruppe_ust_idx( this, "gruppe_ust_idx" )
{
  add_key( new Forkey( this, &gruppe_ust_idx, "GRP_UST", "idx" ) );
  add_key( new Forkey( this, &gruppe_ust, "GRP_UST", "name" ) );
}

PREIS::PREIS()
  : BASE( "PREIS", this ),
	art_artnr( this, "art_artnr", NAME_LEN ),
	art_idx( this, "art_idx" ),
	preis( this, "preis" ),
	bezeichnung( this, "bezeichnung", BEZEICHNUNG_LEN ),
	bezeichnung2( this, "bezeichnung2", BEZEICHNUNG_LEN ),
	gutschein( this, "gutschein" ),
	menuepreis( this, "menuepreis" )
{
  gutschein.data = JANEIN::NEIN;
  menuepreis.data = JANEIN::NEIN;

  add_key( new Forkey( this, &art_artnr, "ART", "artnr" ) );
  add_key( new Forkey( this, &art_idx, "ART", "idx" ) );
}

USER::USER()
  : BASE( "USER", this ),
	login( this, "login", NAME_LEN ),
	vor_name( this, "vor_name", NAME_LEN ),
	nach_name( this, "nach_name", NAME_LEN ),
	passwd( this, "passwd", NAME_LEN )
{
}

GRP_ART::GRP_ART()
  : DBBindType( "GRP_ART" ),
	idx( this, "idx" ),
	name( this, "name", NAME_LEN ),
	bezeichnung( this, "bezeichnung", BEZEICHNUNG_LEN ),
	bezeichnung2( this, "bezeichnung2", BEZEICHNUNG_LEN ),
	art_artnr( this, "art_artnr", NAME_LEN ),
	art_idx( this, "art_idx" ) 
{
  add_key( new Forkey( this, &art_artnr, "ART", "artnr" ) );
  add_key( new Forkey( this, &art_idx, "ART", "idx" ) );
}

LAYOUTK::LAYOUTK()
  : DBBindType( "LAYOUTK" ),
	idx( this, "idx" ),
	name( this, "name", LAYOUT_NAME_LEN ),
	width( this, "width" ),
	height( this, "height" ),
	color( this, "color" )
{}

LAYOUTP::LAYOUTP()
  : DBBindType( "LAYOUTP" ),
	idx( this, "idx" ),
	layout_idx( this, "layout_idx" ),
	label_name( this, "label_name", LAYOUT_NAME_LEN ),
	label_x( this, "label_x" ),
	label_y( this, "label_y" ),
	label_width( this, "label_width" ),
	label_height( this, "label_height" ),
	label_color( this, "label_color" ),
	label_icon( this, "label_icon", LAYOUT_NAME_LEN )
{
  add_key( new Forkey( this, &layout_idx, "LAYOUTK", "idx" ) );
}


BONLINE::BONLINE()
  : BASE( "BONLINE", this ),
	user( this, "user", USER_NAME_LEN ),
	user_idx( this, "user_idx" ),
	artnr( this, "artnr", NAME_LEN ),
	artnr_idx( this, "artnr_idx" ),
	bill_printed( this, "bill_printed" ),
	bill_number( this, "bill_number", BILL_NAME_LEN ),
	layoutk_idx( this, "layoutk_idx" ),
	layoutk_name( this, "layoutk_name", LAYOUT_NAME_LEN ),
	layoutp_idx( this, "layoutp_idx" ),
	layoutp_label_name( this, "layoutp_label_name", LAYOUT_NAME_LEN ),
	tagesabschluss( this, "tagesabschluss" ),
	abschl_zeit( this, "abschl_zeit" ),
	abschl_user_idx( this, "abschl_user_idx" ),
	abschl_user_name( this, "abschl_user_name", USER_NAME_LEN ),
	storniert( this, "storniert" ),
	storno_user( this, "storno_user" ),
	storno_user_idx( this, "storno_user_idx" ),
	anzahl( this, "anzahl" ),
	preis_idx( this, "preis_idx" ),
	preis( this, "preis" ),
	bar_bezahlt( this, "bar_bezahlt" ),
	sonst_bezahlt( this, "sonst_bezahlt" ),
	sonst_art_name( this, "sonst_art_name", NAME_LEN ),
	sonst_art_idx( this, "sonst_art_idx" ),
	bezahlt( this, "bezahlt" )
{
  bill_printed = JANEIN::NEIN;
  tagesabschluss = JANEIN::NEIN;
  storniert = JANEIN::NEIN;
  bezahlt = JANEIN::NEIN;

  add_key( new Forkey( this, &user, "USER", "login" ) );
  add_key( new Forkey( this, &user_idx, "USER", "idx" ) );
  add_key( new Forkey( this, &layoutk_idx, "LAYOUTK", "idx" ) );
  add_key( new Forkey( this, &layoutk_name, "LAYOUTK", "name" ) );
  add_key( new Forkey( this, &layoutp_idx, "LAYOUTP", "idx" ) );
  add_key( new Forkey( this, &layoutp_label_name, "LAYOUTP", "label_name" ) );
  // add_key( new Forkey( this, &preis_idx, "PREIS", "idx" ) );
  // add_key( new Forkey( this, &preis, "PREIS", "preis" ) );
  // add_key( new Forkey( this, &sonst_art_name, "SONST_ART", "name" ) );
  // add_key( new Forkey( this, &sonst_art_idx, "SONST_ART", "idx" ) );
}
  

SONST_ART::SONST_ART()
  : BASE( "SONST_ART", this ),
	name( this, "name", NAME_LEN ),
	bezeichnung( this, "bezeichnung", BEZEICHNUNG_LEN ),
	bezeichnung2( this, "bezeichnung2", BEZEICHNUNG_LEN ),
	wert( this, "wert" )
{

}

LANG::LANG()
  : DBBindType( "LANG" ),
	TableType( TableType::MYISAM ),
	idx( this, "idx" ),
	system( this, "system", SYSTEM_NAME_LEN ),
	type( this, "type", TYPE_NAME_LEN ),
	orig( this, "orig", TRANS_NAME_LEN ),
	trans( this, "trans", TRANS_NAME_LEN )
{
  
}


CONFIG::CONFIG()
  : DBBindType( "CONFIG" ),
	TableType( TableType::MYISAM ),
	idx( this, "idx" ),
	system( this, "system", CONFIG_SYSTEM_LEN ),
	key( this, "key", CONFIG_KEY_LEN ),
	value( this, "value", CONFIG_VALUE_LEN ),
	descr( this, "descr", CONFIG_DESCR_LEN )
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
  GRP_UST grp_ust;
  USER user;
  ART art;
  GRP_ART grp_art;
  LAYOUTK layoutk;
  LAYOUTP layoutp;
  BONLINE bonline;
  PREIS preis;
  SONST_ART sonst_art;
  LANG lang;
  CONFIG config;

  std::vector< Ref<Forkey> > forkeys;

  s += create_sql_statement( &grp_ust, forkeys );
  s += create_sql_statement( &user, forkeys  );
  s += create_sql_statement( &art, forkeys  );
  s += create_sql_statement( &grp_art, forkeys  );
  s += create_sql_statement( &layoutk, forkeys  );
  s += create_sql_statement( &layoutp, forkeys  );
  s += create_sql_statement( &bonline, forkeys  );
  s += create_sql_statement( &preis, forkeys  );
  s += create_sql_statement( &sonst_art, forkeys  );
  s += create_sql_statement( &lang, forkeys  );
  s += create_sql_statement( &config, forkeys  );

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

