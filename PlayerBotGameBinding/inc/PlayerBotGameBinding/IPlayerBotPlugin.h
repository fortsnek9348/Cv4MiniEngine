#pragma once

#include <memory>
#include <string_view>

namespace cvbot
{
	class IBot;
	class IBotInit;

	class IPlayerBotPlugin
	{
	public:
		virtual std::wstring_view getName() const = 0;
		virtual std::wstring_view getModName() const = 0;
		virtual std::wstring_view getAutoStartPlayerName() const = 0;
		virtual std::wstring_view getAutoStartGameName() const = 0;
		virtual std::unique_ptr<IBot> createBot(const IBotInit& botInit) const = 0;
		
		virtual ~IPlayerBotPlugin() = default;
	};
}