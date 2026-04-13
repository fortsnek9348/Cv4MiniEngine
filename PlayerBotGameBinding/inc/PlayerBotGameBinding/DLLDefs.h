#pragma once

#include <array>
#include <mdspan>

namespace cvbot
{
	// Hard-coded DLL values. All this stuff should be checked by DLL

	inline constexpr int kMaxPlayers = 19;
	inline constexpr int kMaxTeams = 19;
	inline constexpr int kMaxCivPlayers = 18;
	inline constexpr int kMaxCivTeams = 18;

	// This is the only constraint on info type enums. Needed to keep Unit trivial whilst still listing promotions.
	inline constexpr int kAPIMaxPromotions = 64;

	enum EPlayer : std::int8_t;
	enum ETeam : std::int8_t;
	enum class EUnitId : int;

	using CommandUnitGroup = std::span<const EUnitId>;

	inline constexpr auto kNoPlayer = static_cast<EPlayer>(-1);
	inline constexpr auto kNoTeam = static_cast<ETeam>(-1);

	enum class EPlotType : int8_t
	{
		None = -1, // Used for unrevealed, out of bounds
		Peak,
		Hills,
		Land,
		Ocean,
		Num,
	};

	enum class EActivity : int8_t
	{
		None = -1,
		Awake,
		Hold,
		Sleep,
		Heal,
		Sentry,
		Intercept,
		Mission,
		Patrol,
		Plunder,
		Num,
	};

	enum class EAutomation : int8_t
	{
		None = -1,
		Build,
		Network,
		City,
		Explore,
		Religion,
		Num,
	};

	// NOTE: Modified, doesn't match DLL.
	enum class EMission : uint8_t
	{
		MoveTo,
		RouteTo,
		Skip,
		Sleep,
		Fortify,
		Plunder,
		AirPatrol,
		SeaPatrol,
		Heal,
		Sentry,
		Airlift,
		Nuke,
		Recon,
		Paradrop,
		Airbomb,
		RangeAttack,
		Bombard,
		Pillage,
		Sabotage,
		Destroy,
		StealPlans,
		Found,
		Spread,
		SpreadCorporation,
		Join,
		Construct,
		Discover,
		Hurry,
		Trade,
		GreatWork,
		Infiltrate,
		GoldenAge,
		Build,
		Lead,
		Espionage,
	};

	namespace enums
	{
		namespace GameOption
		{
			enum Enum
			{
				AdvancedStart,
				NoCityRazing,
				NoCityFlipping,
				FlippingAfterConquest,
				NoBarbarians,
				RagingBarbarians,
				AggressiveAI,
				LeadAnyCiv,
				RandomPersonalities,
				PickReligion,
				NoTechTrading,
				NoTechBrokering,
				PermanentAlliances,
				AlwaysWar,
				AlwaysPeace,
				OneCityChallenge,
				NoChangingWarPeace,
				NewRandomSeed,
				LockMods,
				CompleteKills,
				NoVassalStates,
				NoGoodyHuts,
				NoEvents,
				NoEspionage,
				Num,
			};
		}

		namespace Yield
		{
			enum Enum : std::int8_t
			{
				None = -1,
				Food,
				Production,
				Commerce,
				Num,
			};
		};

		namespace Commerce
		{
			enum Enum : std::int8_t
			{
				None = -1,
				Gold,
				Research,
				Culture,
				Espionage,
				Num,
			};
		};

		// CvGameTextMgr::getAttitudeString
		namespace AttitudeModifier
		{
			enum Enum
			{
				CloseBorders,
				War,
				Peace,
				SameReligion,
				DifferentReligion,
				BonusTrade,
				OpenBorders,
				DefensivePact,
				RivalDefensivePact,
				RivalVassal,
				ShareWar,
				FavouriteCivic,
				Trade,
				RivalTrade,
				Colony,
				Extra,
				Memory, // Sum of all memory attitudes
				Num,
			};
		}

		namespace Attitude
		{
			enum Enum
			{
				Furious,
				Annoyed,
				Cautious,
				Pleased,
				Friendly,
				Num,
			};
		}
	}

	using EGameOption = enums::GameOption::Enum;
	using EYield = enums::Yield::Enum;
	using ECommerce = enums::Commerce::Enum;
	using EAttitudeModifier = enums::AttitudeModifier::Enum;
	using EAttitude = enums::Attitude::Enum;

	inline constexpr int kNumCityWorkPlots = 21;

	struct ivec2;

	extern const std::array<ivec2, kNumCityWorkPlots> kCityWorkPlotCoords;

	template<class T>
	using Span2D = std::mdspan<T, std::dextents<int, 2>>;
}