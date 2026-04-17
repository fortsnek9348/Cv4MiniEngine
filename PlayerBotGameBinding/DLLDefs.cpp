#include "inc/PlayerBotGameBinding/DLLDefs.h"
#include "inc/PlayerBotGameBinding/GameStructs.h"

using namespace cvbot;

constexpr std::array<ivec2, kNumCityWorkPlots> cvbot::kCityWorkPlotCoords{ {
	{ 0 , 0	 },
	{ 0 , 1	 },
	{ 1 , 1	 },
	{ 1 , 0	 },
	{ 1 , -1 },
	{ 0 , -1 },
	{ -1, -1 },
	{ -1, 0	 },
	{ -1, 1	 },
	{ 0 , 2	 },
	{ 1 , 2	 },
	{ 2 , 1	 },
	{ 2 , 0	 },
	{ 2 , -1 },
	{ 1 , -2 },
	{ 0 , -2 },
	{ -1, -2 },
	{ -2, -1 },
	{ -2, 0	 },
	{ -2, 1	 },
	{ -1, 2	 },
} };