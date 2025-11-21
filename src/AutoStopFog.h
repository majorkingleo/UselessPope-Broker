#pragma once

#include "BasicThread.h"

class AutoStopFog : public BasicThread
{    
public:

    AutoStopFog();

    void run() override;

private:

    void auto_deactivate();
};