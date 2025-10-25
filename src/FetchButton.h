#pragma once

#include "BasicThread.h"
#include "bindtypes.h"
#include <map>
#include <chrono>

class FetchButton : public BasicThread
{
	std::map<std::string,USERS_ACTION> m_users_actions_by_mac_address;
    std::map<std::string,USERS_ACTION> m_users_actions_by_username;

public:
    FetchButton();

    void run() override;

protected:
    void fetch_buttons();
    void fetch_users();
};
