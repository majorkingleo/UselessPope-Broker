#pragma once

#include "BasicThread.h"
#include <string>

// Passively listens for Battlefield 1942 LAN server beacons (UDP broadcasts the
// server sends to 255.255.255.255:54915) and records each discovered server in
// the BF1942_SERVER registry table.
//
// Reference implementation: UselessPope/bf1942-adjust-monitor-settings
// src_bf1942_discover/bf1942_discover.cc.
class Bf1942Discover : public BasicThread
{
private:
	unsigned m_port;

public:
	Bf1942Discover( unsigned port );

	void run() override;

private:
	void received_data( const std::string & data, const std::string & source_ip );
};
