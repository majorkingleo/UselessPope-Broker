#include "Bf1942Discover.h"

#include <boost/asio.hpp>
#include <array>
#include <cstring>
#include <ctime>
#include <exception>
#include <functional>
#include <utility>

#include <CpputilsDebug.h>
#include <format.h>
#include <string_utils.h>
#include <stderr_exception.h>
#include <dbi.h>

#include "App.h"
#include "bindtypes.h"

using namespace Tools;
using boost::asio::ip::udp;
using namespace std::chrono_literals;

namespace {

struct Beacon
{
	std::string name;
	std::string guid;
};

// The LAN announce (UDP 54915 -> 255.255.255.255) is a NUL-terminated server
// name starting at byte 1, followed by opaque binary data that happens to
// contain the server GUID as a "{...}" string.
bool parse_beacon( const char * data, std::size_t len, Beacon & out )
{
	if( len < 2 )
		return false;

	const std::size_t name_max = len - 1;
	std::size_t i = 0;
	while( i < name_max && data[1 + i] != '\0' )
		++i;
	out.name.assign( data + 1, i );

	const char * g = static_cast<const char *>( std::memchr( data, '{', len ) );
	if( g != nullptr ) {
		const char * e = static_cast<const char *>( std::memchr( g, '}', len - static_cast<std::size_t>( g - data ) ) );
		if( e != nullptr )
			out.guid.assign( g, static_cast<std::size_t>( e - g ) + 1 );
	}
	return !out.name.empty();
}

// Same timestamp format BASE::setHist() writes into the hist_* columns, which
// MariaDB accepts for a DATETIME column.
std::string now_string()
{
	std::time_t t = std::time( nullptr );
	std::tm * tm = std::localtime( &t );

	return format( "%d-%02d-%02d %02d:%02d:%02d",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec );
}

} // namespace

Bf1942Discover::Bf1942Discover( unsigned port )
: BasicThread( "Bf1942Discover" ),
  m_port( port )
{

}

void Bf1942Discover::run()
{
	boost::asio::io_context io_context;
	udp::socket socket( io_context, udp::endpoint( udp::v4(), m_port ) );
	socket.set_option( udp::socket::reuse_address( true ) );

	std::array<char, 1500> buffer {};
	udp::endpoint remote {};

	std::function<void()> start_receive;

	start_receive = [&]() {
		socket.async_receive_from(
			boost::asio::buffer( buffer ), remote,
			[&]( const boost::system::error_code & error, std::size_t bytes_transferred ) {
				if( !error && bytes_transferred > 0 ) {
					// Raw bytes: the beacon is binary (NULs + leaked heap
					// pointers), so it must not be stripped like a text line.
					// A DB failure must not take the whole daemon down (an
					// exception escaping this handler would also skip the
					// re-arm below), so it is logged and the loop continues.
					try {
						received_data(
							std::string( buffer.data(), bytes_transferred ),
							remote.address().to_string() );
					} catch( const std::exception & e ) {
						CPPDEBUG( format( "BF1942 discovery error: %s", e.what() ) );
					}
				}
				start_receive();
			} );
	};

	start_receive();

	while( !APP.quit_request ) {
		io_context.run_for( 300ms );
	}
}

void Bf1942Discover::received_data( const std::string & data, const std::string & source_ip )
{
	Beacon b;
	if( !parse_beacon( data.data(), data.size(), b ) ) {
		return;
	}

	CPPDEBUG( format( "BF1942 beacon: name='%s' guid=%s at %s", b.name, b.guid, source_ip ) );

	const std::string key = !b.guid.empty() ? b.guid : ( b.name + "@" + source_ip );
	const std::string now = now_string();

	BF1942_SERVER existing;
	const bool found = StdSqlSelect( *APP.db,
		format( "select %%%s from %s where `guid` = '%s' ",
			existing.get_table_name(),
			existing.get_table_name(),
			escape( key ) ),
		DBInList<DBBindType>() >> existing ) > 0;

	if( found ) {
		existing.name = b.name;
		existing.ip_address = source_ip;
		existing.last_seen = now;
		existing.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );

		if( !StdSqlUpdate( *APP.db, existing,
				format( "where `%s` = '%d'", existing.idx.get_name(), existing.idx.data ) ) ) {
			throw STDERR_EXCEPTION( format( "cannot update BF1942_SERVER: %s", APP.db->get_error() ) );
		}
	} else {
		BF1942_SERVER srv {};
		srv.name = b.name;
		srv.guid = key;
		srv.ip_address = source_ip;
		srv.last_seen = now;
		srv.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
		srv.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		srv.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );

		if( !StdSqlInsert( *APP.db, srv ) ) {
			throw STDERR_EXCEPTION( format( "cannot insert into BF1942_SERVER: %s", APP.db->get_error() ) );
		}
	}

	APP.db->commit();
}
