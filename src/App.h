#pragma once

#include <atomic>

struct App
{
	std::atomic<bool> quit_request = false;
};

extern App APP;
