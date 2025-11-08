#pragma once

#include <atomic>
#include <database.h>
#include <memory>
#include <functional>

struct App
{
	std::atomic<bool> 					quit_request 		{false};
	Tools::ThreadedDatabase 			db {};
	std::function<void()> 				reconnect_db {};
};

extern App APP;
