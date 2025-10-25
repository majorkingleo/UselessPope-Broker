#pragma once

#include "BasicThread.h"
#include "bindtypes.h"
#include <map>

class FetchButton : public BasicThread
{
	std::map<std::string,USERS_ACTION> m_users_actions_by_mac_address;

public:
    FetchButton();

    void run() override;

protected:
    void fetch_buttons();
    void fetch_users();
};
