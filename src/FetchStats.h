#pragma once

#include "BasicThread.h"
#include "bindtypes.h"

class FetchStats : public BasicThread
{
public:
	static constexpr const char* MOST_PLAYED_SOUND  = "mostplayedsound";
	static constexpr const char* ACTIVE_LEDS_DATA 	= "active-leds-data";
	static constexpr const char* RPMS 				= "umdrehungen";
	static constexpr const char* FREQUENCY 			= "frequenz";
	static constexpr const char* TOP_USER_1 		= "user1";
	static constexpr const char* TOP_USER_2 		= "user2";
	static constexpr const char* TOP_USER_3 		= "user3";
	static constexpr const char* TOTAL_ACTIONS 		= "totalactions";

public:
    FetchStats();

    void run() override;

protected:
    void fetch_total_actions();
    void fetch_top_user_actions();
    void fetch_mostplayed_sound();
    unsigned count_table( const BASE & table );
    STATS fetch_stats( const std::string & key );
};
