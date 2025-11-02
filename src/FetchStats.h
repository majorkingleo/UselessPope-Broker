#pragma once

#include "BasicThread.h"

class FetchStats : public BasicThread
{
public:
	static constexpr const std::string MOST_PLAYED_SOUND = "mostplayedsound";


public:
    FetchStats();

    void run() override;

protected:
    void fetch_most_active();
};
