#include "ButtonListener.h"
#include <boost/asio.hpp>
#include <functional>
#include <CpputilsDebug.h>
#include <format.h>
#include "App.h"
#include <string_utils.h>

using namespace Tools;
using boost::asio::ip::udp;
using namespace std::chrono_literals;

namespace {

class udp_server
{
public:
	using data_func_t = std::function<void(std::string)>;

private:
	udp::socket 						m_socket;
	udp::endpoint 						m_remote_endpoint;
	std::array<char, 4096> 				m_recv_buffer;
	data_func_t 						m_received_data;

public:
	udp_server(boost::asio::io_context& io_context,
			   unsigned port,
			   data_func_t data_func )
	: m_socket(io_context, udp::endpoint(udp::v4(), port )),
	  m_received_data( data_func )
	{
		start_receive();
	}

private:
	void start_receive()
	{
		m_socket.async_receive_from(
				 boost::asio::buffer(m_recv_buffer), m_remote_endpoint,
				 std::bind(&udp_server::handle_receive, this,
						   boost::asio::placeholders::error,
						   boost::asio::placeholders::bytes_transferred));
	}

	void handle_receive(const boost::system::error_code& error,
						std::size_t bytes_transferred )
	{
		if (!error)
		{
			std::string_view data( m_recv_buffer.data(), bytes_transferred );
			auto v_data = strip_view( data );
			CPPDEBUG( Tools::format( "rec: %s", v_data ) );
			m_received_data( std::string(v_data) );
		}

		start_receive();
	}
};

}

ButtonListener::ButtonListener( unsigned port )
: BasicThread( "ButtonListener" ),
  m_port( port )
{

}

void ButtonListener::run()
{
	boost::asio::io_context io_context;
	udp_server server( io_context, m_port, [this]( std::string data ) {
		received_data( data );
	});

	while( !APP.quit_request ) {
		io_context.run_for( 300ms );
	}
}

void ButtonListener::received_data( const std::string & data )
{
	const auto sl_lines = split_and_strip_simple_view( data, "\n" );

	for( const auto & line : sl_lines )
	{
		//TIME=650922;SEQ=161;MAC=D8:BC:38:FA:EF:20;IP=192.168.1.137;ACTION=ButtonReleased
		const auto chunks = split_and_strip_simple_view( line, ";" );

		std::map<std::string,std::string> message;

		for( const auto & chunk : chunks ) {

			// TIME=650922
			const auto key_value = split_and_strip_simple_view( chunk, "=" );

			for( unsigned i = 0; i+1 < key_value.size(); i += 2 ) {
				const std::string_view & key( key_value[i] );
				const std::string_view & value( key_value[i+1] );

				message[std::string(key)] = value;
			} // for
		} // for

		auto o_button_queue = message_to_table( message );

		if( o_button_queue ) {
			if( !StdSqlInsert( *APP.db, *o_button_queue ) ) {
				CPPDEBUG( Tools::format( "SqlError: '%s'", APP.db->get_error() ));
			}
			APP.db->commit();
		}
	}
}

std::optional<BUTTON_QUEUE> ButtonListener::message_to_table( const std::map<std::string,std::string> & message ) const
{
	BUTTON_QUEUE button{};

	button.setHist(BASE::HIST_TYPE::HIST_AN);
	button.setHist(BASE::HIST_TYPE::HIST_AE);
	button.setHist(BASE::HIST_TYPE::HIST_LO);

	static const std::string KEY_TIME 	= "TIME";
	static const std::string KEY_SEQ  	= "SEQ";
	static const std::string KEY_IP   	= "IP";
	static const std::string KEY_MAC  	= "MAC";
	static const std::string KEY_ACTION = "ACTION";

	for( auto & p : message ) {

		// TIME=650922;SEQ=161;MAC=D8:BC:38:FA:EF:20;IP=192.168.1.137;ACTION=ButtonReleased
		if( p.first == KEY_TIME ) {
			button.time_stamp.data = s2x<unsigned>(p.second,0);
		} else if( p.first == KEY_SEQ ) {
			button.seq = s2x<unsigned>(p.second,0);
		} else if( p.first == KEY_IP ) {
			button.ip_address = p.second;
		} else if( p.first == KEY_MAC ) {
			button.mac_address = p.second;
		} else if( p.first == KEY_ACTION ) {
			button.action = p.second;
		} else {
			CPPDEBUG( Tools::format( "unknown key: '%s'", p.first ));
			return {};
		}
	}

	return button;
}
