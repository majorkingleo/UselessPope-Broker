#pragma once

#include <atomic>
#include <database.h>
#include <memory>
#include <functional>

struct App
{
	std::atomic<bool> 					quit_request 		{false};
	std::shared_ptr<Tools::Database> 	db {};

	std::function<void()> 				reconnect_db {};
};

extern App APP;
