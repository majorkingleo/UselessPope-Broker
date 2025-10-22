#pragma once

#include <thread>
#include <list>
#include <mutex>
#include "BasicThread.h"
#include <chrono>
#include "bindtypes.h"
#include <optional>
#include <map>

class ButtonListener : public BasicThread
{
private:
	unsigned	m_port;

public:
	ButtonListener( unsigned port );

	void run() override;

private:

	void received_data( const std::string & data );

	std::optional<BUTTON_QUEUE> message_to_table( const std::map<std::string,std::string> & message ) const;
};
