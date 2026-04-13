#pragma once

class CvPlayerAI;

namespace cvbot
{
	// Only checks that the currently loaded mod is compatible with the API. Does not check if a bot was compiled with a different API.
	[[nodiscard]] bool checkAPI();

	class IBot;

	void handlePopups(CvPlayerAI& player, IBot& bot);
}