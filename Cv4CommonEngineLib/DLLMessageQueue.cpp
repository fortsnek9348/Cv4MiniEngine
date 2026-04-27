#include "inc/Cv4CommonEngineLib/DLLMessageQueue.h"

#include <CvMessageData.h>

//#include <CvGlobals.h>

//#include <iostream>

DLLMessageQueue& DLLMessageQueue::getInstance()
{
	static DLLMessageQueue x;
	return x;
}

void DLLMessageQueue::reset()
{
	mQueue.clear();
}

void DLLMessageQueue::push(std::unique_ptr<CvMessageData> p)
{
	//if (GC.getSynchLogging())
	//{
	//	char buf[256]{};
	//	p->Debug(buf);
	//	std::clog << buf << '\n';
	//}
	mQueue.push_back(std::move(p));
}

bool DLLMessageQueue::execute()
{
	if (mQueue.empty())
		return false;

	while (!mQueue.empty())
	{
		std::unique_ptr<CvMessageData> p = std::move(mQueue.front());
		mQueue.erase(mQueue.begin());

		p->Execute();
	}

	return true;
}
