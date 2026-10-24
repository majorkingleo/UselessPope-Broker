#pragma once

#include "BasicThread.h"
#include <string>

// Passively listens for Battlefield 1942 LAN server beacons (UDP broadcasts the
// server sends to 255.255.255.255:54915) and records each discovered server in
// the BF1942_SERVER registry table.
//
// Reference implementation: UselessPope/bf1942-adjust-monitor-settings
// src_bf1942_discover/bf1942_discover.cc.
//
// All messages from this task go to the shared ~/.broker/bf-network.log (the
// same file the BF1942 query task uses), not to the main broker log.
class Bf1942Discover : public BasicThread
{
public:
	Bf1942Discover( unsigned port );

	void run() override;

private:
	void received_data( const std::string & data, const std::string & source_ip );
	void log_message( const char * file, unsigned line, const char * function, const std::string & message );

	unsigned m_port;
};
