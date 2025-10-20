#pragma once

#include "BasicThread.h"
#include "PlaySound.h"

class FetchSound : public BasicThread
{
private:
    PlaySound & play_sound;

public:

    FetchSound( PlaySound & ps );

    void run() override;

protected:
    void fetch_music();
    void fetch_chunk();
};