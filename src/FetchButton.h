#pragma once

#include "BasicThread.h"
#include "bindtypes.h"
#include <map>

class FetchButton : public BasicThread
{
	std::map<std::string,USER> m_users;

public:
    FetchButton();

    void run() override;

protected:
    void fetch_buttons();
    void fetch_users();
};
