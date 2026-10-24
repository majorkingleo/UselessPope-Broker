#pragma once

#include "BasicThread.h"
#include "bindtypes.h"
#include <map>
#include <string>
#include <vector>

// Periodically queries Battlefield 1942 servers over the GameSpy/ASE status
// protocol (UDP `\status\`) and records the live scoreboard in BF1942_STATUS
// and BF1942_PLAYER.
//
// Reference implementation: UselessPope/bf1942-adjust-monitor-settings
// src_bf1942_query/bf1942_query.cc.
//
// All messages go to the shared ~/.broker/bf-network.log, not to broker.log.
class Bf1942Query : public BasicThread
{
public:
	struct Target { std::string host; int port; };

	Bf1942Query( int default_port, int interval_s, int timeout_s, const std::string & hosts );

	void run() override;

	// One-shot query for the -query-bf1942 CLI: prints the scoreboard and
	// returns false on failure.
	static bool query_once( const std::string & host, int port, int timeout_s );

private:
	std::vector<Target> targets_from_db();
	void query_and_store( const Target & target );
	void finalize_round( const std::string & server_ip, const BF1942_STATUS & old );

	void store_status( const std::string & server_ip,
	                   const std::map<std::string,std::string> & fields,
	                   const BF1942_STATUS & old,
	                   bool has_old,
	                   bool round_ended );
	void store_players( const std::string & server_ip, const std::map<std::string,std::string> & fields );

	void log_message( const char * file, unsigned line, const char * function, const std::string & message );

	int m_default_port;
	int m_interval_s;
	int m_timeout_s;
	std::vector<Target> m_static_targets;
};
