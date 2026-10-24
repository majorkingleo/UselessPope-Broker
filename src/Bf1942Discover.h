#pragma once

#include "BasicThread.h"
#include <memory>
#include <string>

namespace AsyncOut {
class FileLogger;
class Debug;
}

// Passively listens for Battlefield 1942 LAN server beacons (UDP broadcasts the
// server sends to 255.255.255.255:54915) and records each discovered server in
// the BF1942_SERVER registry table.
//
// Reference implementation: UselessPope/bf1942-adjust-monitor-settings
// src_bf1942_discover/bf1942_discover.cc.
//
// All messages from this task go to their own ~/.broker/bf-network.log (same
// 10 MB cap as broker.log), not to the main broker log.
class Bf1942Discover : public BasicThread
{
public:
	Bf1942Discover( unsigned port );
	~Bf1942Discover();

	void run() override;

private:
	void received_data( const std::string & data, const std::string & source_ip );
	void log_message( const char * file, unsigned line, const char * function, const std::string & message );

	unsigned m_port;

	// Declaration order matters: the frontend (publisher) must outlive the
	// backend (subscriber), so ~Bf1942Discover resets them in that order.
	std::unique_ptr<AsyncOut::Debug>      m_debug;
	std::unique_ptr<AsyncOut::FileLogger> m_file_log;
};
