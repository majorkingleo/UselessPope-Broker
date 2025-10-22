#pragma once

#include <thread>
#include <list>
#include <mutex>
#include "BasicThread.h"
#include <chrono>

class ButtonListener : public BasicThread
{
private:
	unsigned					 	m_port;

public:
	ButtonListener( unsigned port );

	void run() override;

private:

	void received_data( const std::string & data );
};
