#pragma once

#include "BasicThread.h"
#include "bindtypes.h"
#include <chrono>
#include <deque>
#include <map>
#include <string>
#include <vector>

class FetchButton : public BasicThread
{
    static const std::string ACTION_BUTTON_PRESSED;
    static const std::string ACTION_BUTTON_RELEASED;
    static const std::string ACTION_PING;

    // Maximum number of clicks recognised in one burst. The N-th click plays the
    // N-th file (case-insensitively sorted) from the user's audio_main directory.
    static constexpr unsigned MAX_CLICK_COUNT = 5;

    // One button event seen recently, used to drop the duplicate UDP datagrams
    // the remote button sends (identical MAC + SEQ + TIME).
    struct SeenButtonEvent {
        unsigned seq {};
        unsigned time_stamp {};
        std::chrono::steady_clock::time_point seen_at {};
    };

    // A run of ButtonReleased events belonging to the same multi-click.
    struct ClickBurst {
        USERS_ACTION user {};
        unsigned count = 0;
        unsigned last_time_stamp = 0;
        std::chrono::steady_clock::time_point last_event {};
    };

	std::map<std::string,USERS_ACTION> m_users_actions_by_mac_address;
    std::map<std::string,USERS_ACTION> m_users_actions_by_username;

    std::map<std::string,std::deque<SeenButtonEvent>> m_seen_button_events;
    std::map<std::string,ClickBurst> m_click_bursts;

public:
    FetchButton();

    void run() override;

protected:
    void fetch_buttons();
    void fetch_users();

    bool delete_button_queue_entry( BUTTON_QUEUE & bq );

    bool is_duplicate_button_event( const BUTTON_QUEUE & bq );
    void handle_button_click( const BUTTON_QUEUE & bq, const USERS_ACTION & user );
    void flush_click_burst( const ClickBurst & burst );
    void flush_expired_click_bursts();

    std::vector<std::string> get_audio_main_files( const std::string & home_directory ) const;
};
