#pragma once

#include "BasicThread.h"
#include "PlayAnimation.h"

class FetchAnimation : public BasicThread
{
private:
    PlayAnimation & play_animation;

public:

    FetchAnimation( PlayAnimation & pa );

    void run() override;

protected:
    void fetch_animations();
};
