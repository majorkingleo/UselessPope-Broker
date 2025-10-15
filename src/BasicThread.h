#pragma once

class BasicThread
{
protected:
	std::string m_name;

public:
	BasicThread( const std::string & name )
	: m_name( name )
	{}

	~BasicThread() = default;

	virtual void run() = 0;
};
