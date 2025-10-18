#pragma once

#include <atomic>
#include <database.h>
#include <memory>

struct App
{
	std::atomic<bool> quit_request = false;
	std::shared_ptr<Tools::Database> db {};
};

extern App APP;
