#pragma once

#include <memory>
#include <vector>

class CvMessageData;

class DLLMessageQueue
{
public:
	static DLLMessageQueue& getInstance();

	// We'll call this when starting a new game.
	void reset();

	void push(std::unique_ptr<CvMessageData>);

	// Returns true iff something happened.
	bool execute();

private:
	std::vector<std::unique_ptr<CvMessageData>> mQueue;
};