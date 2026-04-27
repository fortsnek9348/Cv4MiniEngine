#pragma once

#include "CvApp.h"

namespace cvengine
{
	class AppMainMenusState : public ICvAppState
	{
	public:
		virtual void onEnter(CvApp&) override;
		virtual void onUpdate(CvApp&) override;
		virtual void onLeave(CvApp&) override;
	};
}