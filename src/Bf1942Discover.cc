#include "Bf1942Discover.h"

#include <boost/asio.hpp>
#include <algorithm>
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
#include "logging.h"

using namespace Tools;
using boost::asio::ip::udp;
using namespace std::chrono_literals;

namespace {

struct Beacon
{
	std::string name;
	std::string guid;
};

// Keep only valid UTF-8, drop control bytes, and never exceed max_bytes (so the
// value fits the VARCHAR column without splitting a multi-byte sequence).
std::string sanitize_utf8( const std::string & in, std::size_t max_bytes )
{
	std::string out;
	out.reserve( std::min( in.size(), max_bytes ) );

	for( std::size_t i = 0; i < in.size() && out.size() < max_bytes; ) {
		const unsigned char c = static_cast<unsigned char>( in[i] );
		std::size_t n = 0;

		if( c < 0x80 ) n = 1;
		else if( ( c & 0xE0 ) == 0xC0 ) n = 2;
		else if( ( c & 0xF0 ) == 0xE0 ) n = 3;
		else if( ( c & 0xF8 ) == 0xF0 ) n = 4;
		else { ++i; continue; }          // invalid lead byte: drop

		if( i + n > in.size() ) break;
		if( out.size() + n > max_bytes ) break;   // don't split a sequence

		bool ok = true;
		for( std::size_t k = 1; k < n; ++k ) {
			if( ( static_cast<unsigned char>( in[i + k] ) & 0xC0 ) != 0x80 ) {
				ok = false;
				break;
			}
		}
		if( !ok ) { ++i; continue; }     // invalid continuation: resync

		if( n == 1 && c < 0x20 ) { ++i; continue; }  // drop control chars

		out.append( in, i, n );
		i += n;
	}

	return out;
}

bool looks_like_guid( const char * s, std::size_t len )
{
	if( len != 36 )
		return false;

	for( std::size_t i = 0; i < len; ++i ) {
		const char c = s[i];
		const bool hex = ( c >= '0' && c <= '9' ) ||
		                 ( c >= 'a' && c <= 'f' ) ||
		                 ( c >= 'A' && c <= 'F' );
		if( !hex && c != '-' )
			return false;
	}
	return true;
}

// The LAN announce (UDP 54915 -> 255.255.255.255) is a NUL-terminated server
// name starting at byte 1, followed by opaque binary data that happens to
// contain the server GUID as a "{<36 hex/dash>}" string. The opaque part can
// contain stray '{' bytes (leaked pointers), so pick the first brace pair whose
// body actually looks like a GUID instead of trusting the first '{'.
bool parse_beacon( const char * data, std::size_t len, Beacon & out )
{
	if( len < 2 )
		return false;

	const std::size_t name_max = len - 1;
	std::size_t i = 0;
	while( i < name_max && data[1 + i] != '\0' )
		++i;
	out.name = sanitize_utf8( std::string( data + 1, i ), BF1942_SERVER_NAME_LEN );

	for( std::size_t p = 0; p + 1 < len; ++p ) {
		if( data[p] != '{' )
			continue;

		const char * e = static_cast<const char *>( std::memchr( data + p, '}', len - p ) );
		if( e == nullptr )
			continue;

		const std::size_t body_len = static_cast<std::size_t>( e - ( data + p ) ) - 1;
		if( looks_like_guid( data + p + 1, body_len ) ) {
			out.guid.assign( data + p, body_len + 2 );
			break;
		}
	}

	return !out.name.empty();
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
						log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
							Tools::format( "BF1942 discovery error: %s", e.what() ) );
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

	log_message( __FILE__, __LINE__, __PRETTY_FUNCTION__,
		Tools::format( "BF1942 beacon: name='%s' guid=%s at %s", b.name, b.guid, source_ip ) );

	const std::string key = !b.guid.empty()
		? b.guid
		: sanitize_utf8( b.name + "@" + source_ip, BF1942_GUID_LEN );
	const std::string now = now_string();

	BF1942_SERVER existing;
	const bool found = StdSqlSelect( *APP.db,
		Tools::format( "select %%%s from %s where `guid` = '%s' ",
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
				Tools::format( "where `%s` = '%d'", existing.idx.get_name(), existing.idx.data ) ) ) {
			throw STDERR_EXCEPTION( Tools::format( "cannot update BF1942_SERVER: %s", APP.db->get_error() ) );
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
			throw STDERR_EXCEPTION( Tools::format( "cannot insert into BF1942_SERVER: %s", APP.db->get_error() ) );
		}
	}

	APP.db->commit();
}

void Bf1942Discover::log_message( const char * file, unsigned line, const char * function, const std::string & message )
{
	if( Tools::Debug * bf = bf_network_log() ) {
		bf->add( file, line, function, message );
	} else if( Tools::x_debug ) {
		// Dedicated log unavailable: fall back to the main logger.
		Tools::x_debug->add( file, line, function, message );
	}
}
