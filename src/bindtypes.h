#pragma once

#include <dbi.h>

using namespace Tools;

const unsigned USER_NAME_LEN=20;
const unsigned TIME_LEN=19;
const unsigned NAME_LEN=20;
const unsigned LAYOUT_NAME_LEN=20;
const unsigned BEZEICHNUNG_LEN=50;
const unsigned BILL_NAME_LEN=20;
const unsigned SYSTEM_NAME_LEN=10;
const unsigned TRANS_NAME_LEN=100;
const unsigned TYPE_NAME_LEN=20;
const unsigned CONFIG_SYSTEM_LEN=20;
const unsigned CONFIG_KEY_LEN=20;
const unsigned CONFIG_VALUE_LEN=100;
const unsigned CONFIG_DESCR_LEN=100;

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
  DBTypeEnum< EnumRange<JANEIN> > gueltig;
  DBTypeDateTime gueltig_von;
  DBTypeDateTime gueltig_bis;
  DBTypeDateTime hist_an_zeit;
  DBTypeVarChar  hist_an_user;
  DBTypeDateTime hist_ae_zeit;
  DBTypeVarChar  hist_ae_user;  
  DBTypeDateTime hist_lo_zeit; unsigned count_erg( const std::string &table_name );
  DBTypeVarChar  hist_lo_user;  

  BASE( const std::string & name, DBBindType *base );

  BASE & operator=( const BASE & b )
	{ 
	  DBBindType::operator=(b); 
	  return *this;
	}
};

class GRP_UST : public BASE
{
 public:
  DBTypeVarChar name;
  DBTypeVarChar bezeichnung;
  DBTypeVarChar bezeichnung2;
  DBTypeDouble  ust; // Umsatzsteuer
  
  GRP_UST();

  GRP_UST & operator=( const GRP_UST & b )
	{  
	  BASE::operator=(b); 
	  return *this;
	}
};

class ART : public BASE
{
 public:
  DBTypeVarChar artnr;
  DBTypeVarChar bezeichnung;
  DBTypeVarChar bezeichnung2;
  DBTypeVarChar gruppe_ust;     /* Umsatzsteuergruppe */
  DBTypeInt     gruppe_ust_idx;

  ART();

  ART & operator=( const ART & b )
	{  
	  BASE::operator=(b); 
	  return *this;
	}
};

class PREIS : public BASE
{
 public:
  DBTypeVarChar art_artnr;
  DBTypeInt     art_idx;
  DBTypeDouble  preis;
  DBTypeVarChar bezeichnung;
  DBTypeVarChar bezeichnung2;
  DBTypeEnum< EnumRange<JANEIN> > gutschein;
  DBTypeEnum< EnumRange<JANEIN> > menuepreis; 

  PREIS();
  
  PREIS & operator=( const PREIS & b )
	{  
	  BASE::operator=(b); 
	  return *this;
	}
};

class USER : public BASE
{
 public:
  DBTypeVarChar login;
  DBTypeVarChar vor_name;
  DBTypeVarChar nach_name;
  DBTypeVarChar passwd;

  USER();

  USER & operator=( const USER & b )
	{  
	  BASE::operator=(b); 
	  return *this;
	}
};

class GRP_ART : public DBBindType, public Forkeys
{
 public:
  DBTypeInt     idx;
  DBTypeVarChar name;
  DBTypeVarChar bezeichnung;
  DBTypeVarChar bezeichnung2;
  DBTypeVarChar art_artnr; 
  DBTypeInt     art_idx; 
  
  GRP_ART();

  GRP_ART & operator=( const GRP_ART & b )
	{  
	  DBBindType::operator=(b); 
	  return *this;
	}
};

class LAYOUTK : public DBBindType
{
 public:
  DBTypeInt      idx;
  DBTypeVarChar  name;
  DBTypeInt      width;
  DBTypeInt      height;
  DBTypeInt      color;

  LAYOUTK();
};

class LAYOUTP : public DBBindType, public Forkeys
{
 public:
  DBTypeInt      idx;
  DBTypeInt      layout_idx;
  DBTypeVarChar  label_name;
  DBTypeInt      label_x;
  DBTypeInt      label_y;
  DBTypeInt      label_width;
  DBTypeInt      label_height;
  DBTypeInt      label_color;
  DBTypeVarChar  label_icon;

  LAYOUTP();
};

class BONLINE : public BASE
{
 public:
  DBTypeVarChar  user;
  DBTypeInt      user_idx;
  DBTypeVarChar  artnr;
  DBTypeInt      artnr_idx;
  DBTypeEnum< EnumRange<JANEIN> > bill_printed;
  DBTypeVarChar  bill_number;
  DBTypeInt      layoutk_idx;
  DBTypeVarChar  layoutk_name;
  DBTypeInt      layoutp_idx;
  DBTypeVarChar  layoutp_label_name;
  DBTypeEnum< EnumRange<JANEIN> > tagesabschluss;
  DBTypeDateTime abschl_zeit;
  DBTypeInt      abschl_user_idx;
  DBTypeVarChar  abschl_user_name;
  DBTypeEnum< EnumRange<JANEIN> > storniert;
  DBTypeVarChar  storno_user;
  DBTypeInt      storno_user_idx;
  DBTypeInt      anzahl;
  DBTypeInt      preis_idx;
  DBTypeDouble   preis;
  DBTypeDouble   bar_bezahlt;
  DBTypeDouble   sonst_bezahlt;
  DBTypeVarChar  sonst_art_name;
  DBTypeInt      sonst_art_idx;
  DBTypeEnum< EnumRange<JANEIN> > bezahlt;

  BONLINE();
};

class SONST_ART : public BASE
{
 public:
  DBTypeVarChar  name; 
  DBTypeVarChar  bezeichnung;
  DBTypeVarChar  bezeichnung2;
  DBTypeDouble   wert;

  SONST_ART();
};

class LANG : public DBBindType, public TableType
{
 public:
  DBTypeInt      idx;
  DBTypeVarChar  system;
  DBTypeVarChar  type;
  DBTypeVarChar  orig;
  DBTypeVarChar  trans;

  LANG();
};

class CONFIG : public DBBindType, public TableType
{
 public:
  DBTypeInt      idx;
  DBTypeVarChar  system;
  DBTypeVarChar  key;
  DBTypeVarChar  value;
  DBTypeVarChar  descr;

  CONFIG();
};

std::string create_sql();
std::string create_sql_statement( DBBindType *table );

